/**
 * @file filament.cpp
 * @brief Kantei Grade 3 (Brush) — Google Filament backend.
 *
 * Headless hosts render into an offscreen texture and read RGBA8 back into the
 * framebuffer contract shared with Ink, OpenGL, and direct Metal. EshiView
 * takes the zero-readback path instead: its CVPixelBuffer is a native Filament
 * swapchain, and Flutter composites that same IOSurface.
 */
#include <backend/PixelBufferDescriptor.h>
#include <filament/Box.h>
#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/LightManager.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderTarget.h>
#include <filament/RenderableManager.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/Texture.h>
#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/FilamentInstance.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include <gltfio/materials/uberarchive.h>
#include <math/mat4.h>
#include <math/vec2.h>
#include <math/vec3.h>
#include <utils/EntityManager.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <vector>

#include "../eshi_internal.h"

namespace {

const size_t kUniformFloatCapacity = 64;

static_assert(sizeof(EshiEntity) == sizeof(utils::Entity),
              "Eshi and Filament entity handles must have the same width");
static_assert(ESHI_ENTITY_INDEX_BITS == utils::FILAMENT_GENERATION_SHIFT,
              "Eshi and Filament entity handles must use the same bit layout");

/*
 * Instances an asset may carry. gltfio decides an asset's instance count when
 * it parses, so this is reserved up front and spent by eshi_asset_instance;
 * asking for a ninth copy is a limit, reported as one.
 */
const size_t kMaxAssetInstances = 8;

struct AssetPlacement {
    float x;
    float y;
    float z;
    float scale;
    float radians_per_second;
};

/** One loaded glTF, its reserved instances, and how many are in the scene. */
struct AssetSlot {
    uint32_t id;
    filament::gltfio::FilamentAsset* asset;
    std::vector<filament::gltfio::FilamentInstance*> instances;
    std::vector<AssetPlacement> placements;
    size_t used;
};

struct FilamentBackend {
    int32_t width;
    int32_t height;

    filament::Engine* engine;
    filament::Renderer* renderer;
    filament::Scene* scene;
    filament::View* view;
    filament::Camera* camera;
    filament::Texture* color;
    filament::RenderTarget* target;
    filament::SwapChain* external_swapchain;
    void* external_handle;
    int32_t external_width;
    int32_t external_height;
    filament::VertexBuffer* vertices;
    filament::IndexBuffer* indices;
    filament::Material* material;
    filament::MaterialInstance* material_instance;

    utils::Entity camera_entity;
    utils::Entity renderable;
    std::vector<uint8_t> package;
    std::vector<uint8_t> readback;

    /*
     * The 3D half. It stays null until a host loads an asset, so a world that
     * only draws a fullscreen material builds exactly the scene it built
     * before — which matters, because the cross-tier conformance gate compares
     * that scene against the CPU tier to within one LSB.
     */
    filament::gltfio::MaterialProvider* materials;
    filament::gltfio::AssetLoader* asset_loader;
    filament::gltfio::ResourceLoader* resource_loader;
    filament::gltfio::TextureProvider* stb_textures;
    filament::gltfio::TextureProvider* ktx2_textures;
    utils::Entity sunlight;
    std::vector<AssetSlot> assets;
    uint32_t next_asset_id;
};

bool read_file(const char* path, std::vector<uint8_t>* out) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        return false;

    const std::ifstream::pos_type end = stream.tellg();
    if (end <= 0)
        return false;

    out->resize((size_t)end);
    stream.seekg(0, std::ios::beg);
    return (bool)stream.read(reinterpret_cast<char*>(out->data()), end);
}

int filament_available(void) {
    static const int available = []() {
        try {
            filament::Engine* engine = filament::Engine::create(filament::Engine::Backend::DEFAULT);
            if (!engine)
                return 0;
            filament::Engine::destroy(&engine);
            return 1;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[eshi/filament] backend probe failed: %s\n", error.what());
            return 0;
        } catch (...) {
            std::fprintf(stderr, "[eshi/filament] backend probe failed\n");
            return 0;
        }
    }();
    return available;
}

void filament_destroy(EshiBackend* handle) {
    FilamentBackend* backend = reinterpret_cast<FilamentBackend*>(handle);
    if (!backend)
        return;

    filament::Engine* engine = backend->engine;
    if (engine) {
        /* Assets first: they hold entities the scene and the engine both know. */
        for (size_t i = 0; i < backend->assets.size(); ++i) {
            if (backend->assets[i].asset) {
                backend->scene->removeEntities(backend->assets[i].asset->getEntities(),
                                               backend->assets[i].asset->getEntityCount());
                backend->asset_loader->destroyAsset(backend->assets[i].asset);
            }
        }
        backend->assets.clear();
        if (backend->materials) backend->materials->destroyMaterials();
        delete backend->resource_loader;
        delete backend->stb_textures;
        delete backend->ktx2_textures;
        if (backend->asset_loader) {
            filament::gltfio::AssetLoader::destroy(&backend->asset_loader);
        }
        delete backend->materials;
        if (backend->sunlight) {
            backend->scene->remove(backend->sunlight);
            engine->destroy(backend->sunlight);
            utils::EntityManager::get().destroy(backend->sunlight);
        }
        if (backend->scene && backend->renderable) {
            backend->scene->remove(backend->renderable);
        }
        if (backend->renderable) {
            engine->destroy(backend->renderable);
            utils::EntityManager::get().destroy(backend->renderable);
        }
        if (backend->camera_entity) {
            engine->destroyCameraComponent(backend->camera_entity);
            utils::EntityManager::get().destroy(backend->camera_entity);
        }
        engine->destroy(backend->material_instance);
        engine->destroy(backend->material);
        engine->destroy(backend->vertices);
        engine->destroy(backend->indices);
        engine->destroy(backend->target);
        engine->destroy(backend->color);
        engine->destroy(backend->external_swapchain);
        engine->destroy(backend->view);
        engine->destroy(backend->scene);
        engine->destroy(backend->renderer);
        filament::Engine::destroy(&engine);
    }
    delete backend;
}

EshiBackend* filament_create(int32_t width, int32_t height, const char* /*source_path*/,
                             const char* package_path) {
    /*
     * A missing package is no longer a failure: a world may hold 3D geometry
     * and no fullscreen material at all. What is still a failure is a package
     * that was named and could not be read, which is a build problem wearing a
     * runtime disguise.
     */
    FilamentBackend* backend = new FilamentBackend();
    backend->width = width;
    backend->height = height;
    backend->engine = NULL;
    backend->renderer = NULL;
    backend->scene = NULL;
    backend->view = NULL;
    backend->camera = NULL;
    backend->color = NULL;
    backend->target = NULL;
    backend->external_swapchain = NULL;
    backend->external_handle = NULL;
    backend->external_width = 0;
    backend->external_height = 0;
    backend->vertices = NULL;
    backend->indices = NULL;
    backend->material = NULL;
    backend->material_instance = NULL;
    backend->camera_entity = {};
    backend->renderable = {};
    backend->materials = NULL;
    backend->asset_loader = NULL;
    backend->resource_loader = NULL;
    backend->stb_textures = NULL;
    backend->ktx2_textures = NULL;
    backend->sunlight = {};
    backend->next_asset_id = 1;

    if (package_path && !read_file(package_path, &backend->package)) {
        std::fprintf(stderr, "[eshi/filament] cannot read material package: %s\n", package_path);
        delete backend;
        return NULL;
    }

    try {
        backend->engine = filament::Engine::create(filament::Engine::Backend::DEFAULT);
        if (!backend->engine) {
            std::fprintf(stderr, "[eshi/filament] could not create an engine\n");
            delete backend;
            return NULL;
        }

        filament::Engine& engine = *backend->engine;
        backend->renderer = engine.createRenderer();
        /*
         * Without this the render target keeps whatever was in it — which for a
         * 3D scene is a magenta field and staircase blocks of stale tile memory
         * around the geometry, because a fullscreen quad used to cover every
         * pixel and hide the question. An explicit opaque clear makes the
         * background deterministic, which the headless digests depend on.
         */
        filament::Renderer::ClearOptions clear = {};
        clear.clearColor = {0.02f, 0.027f, 0.051f, 1.0f};
        clear.clear = true;
        backend->renderer->setClearOptions(clear);
        backend->scene = engine.createScene();
        backend->view = engine.createView();

        backend->camera_entity = utils::EntityManager::get().create();
        backend->camera = engine.createCamera(backend->camera_entity);
        backend->view->setCamera(backend->camera);
        backend->view->setScene(backend->scene);
        backend->view->setViewport({0, 0, (uint32_t)width, (uint32_t)height});
        backend->view->setPostProcessingEnabled(false);
        backend->view->setAntiAliasing(filament::View::AntiAliasing::NONE);
        backend->view->setDithering(filament::View::Dithering::NONE);

        backend->color = filament::Texture::Builder()
                                 .width((uint32_t)width)
                                 .height((uint32_t)height)
                                 .levels(1)
                                 .usage(filament::Texture::Usage::COLOR_ATTACHMENT |
                                        filament::Texture::Usage::BLIT_SRC)
                                 .format(filament::Texture::InternalFormat::RGBA8)
                                 .build(engine);
        backend->target =
                filament::RenderTarget::Builder()
                        .texture(filament::RenderTarget::AttachmentPoint::COLOR, backend->color)
                        .build(engine);
        backend->view->setRenderTarget(backend->target);

        /*
         * The fullscreen material is optional now, so everything that serves it
         * — the quad, the package, the renderable — is built only when a host
         * asked for one. A 3D-only world skips straight to its assets.
         */
        if (package_path) {
            static const float kVertices[] = {
                    -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,
            };
            static const uint16_t kIndices[] = {0, 1, 2, 0, 2, 3};

            backend->vertices = filament::VertexBuffer::Builder()
                                        .vertexCount(4)
                                        .bufferCount(1)
                                        .attribute(filament::VertexAttribute::POSITION, 0,
                                                   filament::VertexBuffer::AttributeType::FLOAT2, 0,
                                                   2 * sizeof(float))
                                        .build(engine);
            backend->vertices->setBufferAt(
                    engine, 0,
                    filament::VertexBuffer::BufferDescriptor(kVertices, sizeof(kVertices), NULL));

            backend->indices = filament::IndexBuffer::Builder()
                                       .indexCount(6)
                                       .bufferType(filament::IndexBuffer::IndexType::USHORT)
                                       .build(engine);
            backend->indices->setBuffer(
                    engine, filament::IndexBuffer::BufferDescriptor(kIndices, sizeof(kIndices), NULL));

            backend->material = filament::Material::Builder()
                                        .package(backend->package.data(), backend->package.size())
                                        .build(engine);
            if (!backend->material) {
                std::fprintf(stderr, "[eshi/filament] material package is invalid or lacks this API\n");
                filament_destroy(reinterpret_cast<EshiBackend*>(backend));
                return NULL;
            }
            backend->material_instance = backend->material->createInstance();
            backend->material_instance->setParameter(
                    "eshiResolution", filament::math::float2{(float)width, (float)height});

            backend->renderable = utils::EntityManager::get().create();
            const filament::RenderableManager::Builder::Result result =
                    filament::RenderableManager::Builder(1)
                            .boundingBox({{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}})
                            .material(0, backend->material_instance)
                            .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
                                      backend->vertices, backend->indices, 0, 6)
                            .culling(false)
                            .receiveShadows(false)
                            .castShadows(false)
                            .build(engine, backend->renderable);
            if (result != filament::RenderableManager::Builder::Success) {
                std::fprintf(stderr, "[eshi/filament] could not build fullscreen renderable\n");
                utils::EntityManager::get().destroy(backend->renderable);
                backend->renderable = {};
                filament_destroy(reinterpret_cast<EshiBackend*>(backend));
                return NULL;
            }
            backend->scene->addEntity(backend->renderable);
        }
        backend->readback.resize((size_t)width * (size_t)height * 4);

        std::printf("[eshi/filament] backend=%d package=%s\n", (int)engine.getBackend(),
                    package_path ? package_path : "(none, 3D scene only)");
        return reinterpret_cast<EshiBackend*>(backend);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[eshi/filament] initialization failed: %s\n", error.what());
    } catch (...) {
        std::fprintf(stderr, "[eshi/filament] initialization failed\n");
    }
    filament_destroy(reinterpret_cast<EshiBackend*>(backend));
    return NULL;
}

/*
 * Builds the glTF loaders and the default lighting, once, on the first asset.
 *
 * These are the scene conventions the rest of Step 5 hangs off, so they are
 * stated here rather than left to whatever a caller passes: glTF's own
 * right-handed, Y-up metres; a 45° vertical field of view; a photographic
 * exposure so physical light units mean something; and a single directional
 * light at daylight intensity. Image-based lighting is not here yet, which is
 * why a fully metallic material still renders dark — that is the next bullet,
 * not an accident.
 */
EshiResult ensure_scene(FilamentBackend* backend) {
    if (backend->asset_loader) return ESHI_OK;

    filament::Engine& engine = *backend->engine;
    backend->materials = filament::gltfio::createUbershaderProvider(
            &engine, UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);
    if (!backend->materials) {
        std::fprintf(stderr, "[eshi/filament] could not create the ubershader provider\n");
        return ESHI_ERR_NOMEM;
    }

    filament::gltfio::AssetConfiguration config = {};
    config.engine = &engine;
    config.materials = backend->materials;
    backend->asset_loader = filament::gltfio::AssetLoader::create(config);
    if (!backend->asset_loader) {
        std::fprintf(stderr, "[eshi/filament] could not create the glTF asset loader\n");
        return ESHI_ERR_NOMEM;
    }

    filament::gltfio::ResourceConfiguration resources = {};
    resources.engine = &engine;
    resources.normalizeSkinningWeights = true;
    backend->resource_loader = new filament::gltfio::ResourceLoader(resources);

    /*
     * Without these a textured glTF still loads, still instantiates, and
     * renders black — gltfio drops every image it has no decoder for and says
     * so once on stderr, which is easy to miss and impossible to see in a
     * digest. The greybox asset has no textures, so only a real model exposed
     * it; registering the providers is what makes "loaded" mean "loaded".
     */
    backend->stb_textures = filament::gltfio::createStbProvider(&engine);
    backend->ktx2_textures = filament::gltfio::createKtx2Provider(&engine);
    backend->resource_loader->addTextureProvider("image/png", backend->stb_textures);
    backend->resource_loader->addTextureProvider("image/jpeg", backend->stb_textures);
    backend->resource_loader->addTextureProvider("image/ktx2", backend->ktx2_textures);

    backend->camera->setProjection(45.0, (double)backend->width / (double)backend->height,
                                   0.1, 100.0, filament::Camera::Fov::VERTICAL);
    backend->camera->lookAt({0.0f, 0.35f, 2.4f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    backend->camera->setExposure(16.0f, 1.0f / 125.0f, 100.0f);

    backend->sunlight = utils::EntityManager::get().create();
    filament::LightManager::Builder(filament::LightManager::Type::DIRECTIONAL)
            .color({1.0f, 0.98f, 0.95f})
            .intensity(90000.0f)
            .direction({-0.4f, -0.8f, -0.45f})
            .castShadows(false)
            .build(engine, backend->sunlight);
    backend->scene->addEntity(backend->sunlight);
    return ESHI_OK;
}

AssetSlot* find_asset(FilamentBackend* backend, uint32_t id) {
    for (size_t i = 0; i < backend->assets.size(); ++i) {
        if (backend->assets[i].id == id) return &backend->assets[i];
    }
    return NULL;
}

EshiResult filament_asset_load(EshiBackend* handle, const char* path, uint32_t* out_asset) {
    FilamentBackend* backend = reinterpret_cast<FilamentBackend*>(handle);
    if (!backend || !path || !out_asset) return ESHI_ERR_INVALID;

    std::vector<uint8_t> bytes;
    if (!read_file(path, &bytes)) {
        std::fprintf(stderr, "[eshi/filament] cannot read asset: %s\n", path);
        return ESHI_ERR_INVALID;
    }

    const EshiResult ready = ensure_scene(backend);
    if (ready != ESHI_OK) return ready;

    AssetSlot slot;
    slot.id = 0;
    slot.asset = NULL;
    slot.used = 0;
    /*
     * Instanced from the start. gltfio uploads the geometry and materials once
     * and hands back handles that share them, which is the whole reason a
     * second view or a second copy of the logo costs a transform instead of a
     * second megabyte.
     */
    slot.instances.assign(kMaxAssetInstances, NULL);
    slot.placements.assign(kMaxAssetInstances, AssetPlacement{});
    slot.asset = backend->asset_loader->createInstancedAsset(
            bytes.data(), (uint32_t)bytes.size(), slot.instances.data(), kMaxAssetInstances);
    if (!slot.asset) {
        std::fprintf(stderr, "[eshi/filament] not a glTF this loader accepts: %s\n", path);
        return ESHI_ERR_INVALID;
    }
    if (!backend->resource_loader->loadResources(slot.asset)) {
        std::fprintf(stderr, "[eshi/filament] asset resources failed to load: %s\n", path);
        backend->asset_loader->destroyAsset(slot.asset);
        return ESHI_ERR_INVALID;
    }
    /* The parsed glTF is no longer needed once buffers and textures are up. */
    slot.asset->releaseSourceData();

    slot.id = backend->next_asset_id++;
    backend->assets.push_back(slot);
    *out_asset = slot.id;
    return ESHI_OK;
}

EshiResult filament_asset_instance(EshiBackend* handle, uint32_t asset,
                                   float x, float y, float z, float scale,
                                   float radians_per_second) {
    FilamentBackend* backend = reinterpret_cast<FilamentBackend*>(handle);
    if (!backend) return ESHI_ERR_INVALID;
    AssetSlot* slot = find_asset(backend, asset);
    if (!slot) return ESHI_ERR_INVALID;
    if (slot->used >= slot->instances.size()) return ESHI_ERR_LIMIT;

    filament::gltfio::FilamentInstance* instance = slot->instances[slot->used];
    if (!instance) return ESHI_ERR_INVALID;
    const size_t placement_index = slot->used;
    slot->used += 1;
    slot->placements[placement_index] = {x, y, z, scale, radians_per_second};

    filament::TransformManager& transforms = backend->engine->getTransformManager();
    const filament::TransformManager::Instance root =
            transforms.getInstance(instance->getRoot());
    transforms.setTransform(root,
                            filament::math::mat4f::translation(
                                    filament::math::float3{x, y, z}) *
                                    filament::math::mat4f::scaling(scale));

    backend->scene->addEntities(instance->getEntities(), instance->getEntityCount());
    backend->scene->addEntity(instance->getRoot());
    return ESHI_OK;
}

void animate_asset_instances(FilamentBackend* backend, float time) {
    filament::TransformManager& transforms = backend->engine->getTransformManager();
    for (AssetSlot& slot : backend->assets) {
        for (size_t i = 0; i < slot.used; ++i) {
            filament::gltfio::FilamentInstance* instance = slot.instances[i];
            if (!instance) continue;
            const AssetPlacement& placement = slot.placements[i];
            const filament::TransformManager::Instance root =
                    transforms.getInstance(instance->getRoot());
            transforms.setTransform(
                    root,
                    filament::math::mat4f::translation(
                            filament::math::float3{placement.x, placement.y, placement.z}) *
                    filament::math::mat4f::rotation(
                            time * placement.radians_per_second,
                            filament::math::float3{0.0f, 1.0f, 0.0f}) *
                    filament::math::mat4f::scaling(placement.scale));
        }
    }
}

EshiResult filament_asset_release(EshiBackend* handle, uint32_t asset) {
    FilamentBackend* backend = reinterpret_cast<FilamentBackend*>(handle);
    if (!backend) return ESHI_ERR_INVALID;
    for (size_t i = 0; i < backend->assets.size(); ++i) {
        if (backend->assets[i].id != asset) continue;
        AssetSlot& slot = backend->assets[i];
        backend->scene->removeEntities(slot.asset->getEntities(), slot.asset->getEntityCount());
        backend->asset_loader->destroyAsset(slot.asset);
        backend->assets.erase(backend->assets.begin() + (long)i);
        return ESHI_OK;
    }
    return ESHI_ERR_INVALID;
}

uint32_t filament_asset_count(const EshiBackend* handle) {
    const FilamentBackend* backend = reinterpret_cast<const FilamentBackend*>(handle);
    return backend ? (uint32_t)backend->assets.size() : 0u;
}

EshiResult filament_render(EshiBackend* handle, uint8_t* pixels, int32_t stride, float time,
                           EshiShaderFn /*cpu_shader*/, const void* uniforms, size_t uniform_size) {
    FilamentBackend* backend = reinterpret_cast<FilamentBackend*>(handle);
    if (!backend || !pixels || stride < backend->width * 4)
        return ESHI_ERR_INVALID;
    if ((uniform_size % sizeof(float)) != 0)
        return ESHI_ERR_INVALID;

    const size_t uniform_count = uniform_size / sizeof(float);
    if (uniform_count > kUniformFloatCapacity)
        return ESHI_ERR_LIMIT;
    if (uniform_count > 0 && !uniforms)
        return ESHI_ERR_INVALID;

    /*
     * A 3D-only world has no fullscreen material to feed. Its geometry is
     * already in the scene, so the frame is a plain render of it.
     */
    if (backend->material_instance) {
        backend->material_instance->setParameter("eshiTime", time);
        if (uniform_count > 0) {
            backend->material_instance->setParameter(
                    "eshiUniforms", static_cast<const float*>(uniforms), uniform_count);
        }
    }

    animate_asset_instances(backend, time);

    backend->renderer->renderStandaloneView(backend->view);
    filament::backend::PixelBufferDescriptor readback(
            backend->readback.data(), backend->readback.size(),
            filament::backend::PixelBufferDescriptor::PixelDataFormat::RGBA,
            filament::backend::PixelBufferDescriptor::PixelDataType::UBYTE);
    backend->renderer->readPixels(backend->target, 0, 0, (uint32_t)backend->width,
                                  (uint32_t)backend->height, std::move(readback));
    backend->engine->flushAndWait();

    const size_t source_stride = (size_t)backend->width * 4;
    for (int32_t y = 0; y < backend->height; ++y) {
        uint8_t* destination = pixels + (size_t)y * (size_t)stride;
        std::memcpy(destination, backend->readback.data() + (size_t)y * source_stride,
                    source_stride);
        /* Surface alpha is not meaningful for this opaque framebuffer contract. */
        for (int32_t x = 0; x < backend->width; ++x)
            destination[x * 4 + 3] = 255;
    }
    return ESHI_OK;
}

EshiResult filament_render_texture(EshiBackend* handle,
                                   void* metal_texture,
                                   void* pixel_buffer,
                                   int32_t width, int32_t height,
                                   float time,
                                   const void* uniforms, size_t uniform_size) {
#if !defined(__APPLE__)
    (void)handle;
    (void)metal_texture;
    (void)pixel_buffer;
    (void)width;
    (void)height;
    (void)time;
    (void)uniforms;
    (void)uniform_size;
    return ESHI_ERR_UNSUPPORTED;
#else
    FilamentBackend* backend = reinterpret_cast<FilamentBackend*>(handle);
    (void)metal_texture;
    if (!backend || !pixel_buffer || width <= 0 || height <= 0) {
        return ESHI_ERR_INVALID;
    }
    if ((uniform_size % sizeof(float)) != 0 ||
        uniform_size / sizeof(float) > kUniformFloatCapacity ||
        (uniform_size > 0 && !uniforms)) {
        return ESHI_ERR_INVALID;
    }

    filament::Engine& engine = *backend->engine;
    if (backend->external_handle != pixel_buffer ||
        backend->external_width != width || backend->external_height != height) {
        backend->view->setRenderTarget(backend->target);
        engine.destroy(backend->external_swapchain);
        backend->external_swapchain = engine.createSwapChain(
                pixel_buffer, filament::SwapChain::CONFIG_APPLE_CVPIXELBUFFER);
        if (!backend->external_swapchain) return ESHI_ERR_INVALID;
        backend->external_handle = pixel_buffer;
        backend->external_width = width;
        backend->external_height = height;
    }

    if (backend->material_instance) {
        backend->material_instance->setParameter("eshiTime", time);
        const size_t uniform_count = uniform_size / sizeof(float);
        if (uniform_count > 0) {
            backend->material_instance->setParameter(
                    "eshiUniforms", static_cast<const float*>(uniforms), uniform_count);
        }
    }
    animate_asset_instances(backend, time);

    filament::Renderer::ClearOptions transparent = {};
    transparent.clearColor = {0.0, 0.0, 0.0, 0.0};
    transparent.clear = true;
    backend->renderer->setClearOptions(transparent);
    backend->view->setBlendMode(filament::View::BlendMode::TRANSLUCENT);
    backend->view->setRenderTarget(NULL);
    backend->view->setViewport({0, 0, (uint32_t)width, (uint32_t)height});
    backend->camera->setProjection(45.0, (double)width / (double)height,
                                   0.1, 100.0, filament::Camera::Fov::VERTICAL);
    const bool frame_started =
            backend->renderer->beginFrame(backend->external_swapchain);
    if (frame_started) {
        backend->renderer->render(backend->view);
        backend->renderer->endFrame();
        engine.flushAndWait();
    }

    /* Leave the headless framebuffer contract exactly as it was. */
    filament::Renderer::ClearOptions opaque = {};
    opaque.clearColor = {0.02, 0.027, 0.051, 1.0};
    opaque.clear = true;
    backend->renderer->setClearOptions(opaque);
    backend->view->setBlendMode(filament::View::BlendMode::OPAQUE);
    backend->view->setRenderTarget(backend->target);
    backend->view->setViewport(
            {0, 0, (uint32_t)backend->width, (uint32_t)backend->height});
    backend->camera->setProjection(
            45.0, (double)backend->width / (double)backend->height,
            0.1, 100.0, filament::Camera::Fov::VERTICAL);
    return frame_started ? ESHI_OK : ESHI_ERR_INVALID;
#endif
}

const EshiBackendVTable kFilamentVTable = {
        "filament",
        filament_available,
        filament_create,
        filament_destroy,
        filament_render,
        filament_render_texture,
        filament_asset_load,
        filament_asset_instance,
        filament_asset_release,
        filament_asset_count,
};

} /* namespace */

extern "C" const EshiBackendVTable* eshi__backend_filament(void) { return &kFilamentVTable; }
