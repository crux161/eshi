/**
 * @file filament.cpp
 * @brief Kantei Grade 3 (Brush) — Google Filament backend.
 *
 * Filament renders a device-domain fullscreen material into an offscreen
 * texture, then reads RGBA8 back into the same host-owned framebuffer contract
 * used by Ink, OpenGL, and direct Metal. This readback is intentionally a First
 * Light bridge: later Flutter hosts can expose the texture directly and remove
 * the copy without changing the game or ECS APIs.
 */
#include <backend/PixelBufferDescriptor.h>
#include <filament/Box.h>
#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderTarget.h>
#include <filament/RenderableManager.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/Texture.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <math/vec2.h>
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
    filament::VertexBuffer* vertices;
    filament::IndexBuffer* indices;
    filament::Material* material;
    filament::MaterialInstance* material_instance;

    utils::Entity camera_entity;
    utils::Entity renderable;
    std::vector<uint8_t> package;
    std::vector<uint8_t> readback;
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
        engine->destroy(backend->view);
        engine->destroy(backend->scene);
        engine->destroy(backend->renderer);
        filament::Engine::destroy(&engine);
    }
    delete backend;
}

EshiBackend* filament_create(int32_t width, int32_t height, const char* /*source_path*/,
                             const char* package_path) {
    if (!package_path) {
        std::fprintf(stderr, "[eshi/filament] material has no package_path; compile its .mat "
                             "file with matc\n");
        return NULL;
    }

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
    backend->vertices = NULL;
    backend->indices = NULL;
    backend->material = NULL;
    backend->material_instance = NULL;
    backend->camera_entity = {};
    backend->renderable = {};

    if (!read_file(package_path, &backend->package)) {
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
        backend->readback.resize((size_t)width * (size_t)height * 4);

        std::printf("[eshi/filament] backend=%d package=%s\n", (int)engine.getBackend(),
                    package_path);
        return reinterpret_cast<EshiBackend*>(backend);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[eshi/filament] initialization failed: %s\n", error.what());
    } catch (...) {
        std::fprintf(stderr, "[eshi/filament] initialization failed\n");
    }
    filament_destroy(reinterpret_cast<EshiBackend*>(backend));
    return NULL;
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

    backend->material_instance->setParameter("eshiTime", time);
    if (uniform_count > 0) {
        backend->material_instance->setParameter(
                "eshiUniforms", static_cast<const float*>(uniforms), uniform_count);
    }

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

const EshiBackendVTable kFilamentVTable = {
        "filament", filament_available, filament_create, filament_destroy, filament_render,
};

} /* namespace */

extern "C" const EshiBackendVTable* eshi__backend_filament(void) { return &kFilamentVTable; }
