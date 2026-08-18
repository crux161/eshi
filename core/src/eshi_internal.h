/**
 * @file eshi_internal.h
 * @brief Cross-translation-unit surface inside the core. Never installed.
 *
 * Backends need a handful of facts about the world without depending on its
 * layout, so the world exposes them through a narrow accessor rather than
 * publishing its struct. That keeps render backends swappable — the Filament
 * backend in Phase 1 links against exactly this and nothing more.
 */
#ifndef ESHI_INTERNAL_H
#define ESHI_INTERNAL_H

#include "eshi/eshi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Scene reconciler state
 *
 * The reconciler is a *client* of the public C API — it creates entities and
 * sets components through eshi_entity_create()/eshi_transform_set()/… like any
 * other caller, and so needs nothing from EshiWorld's layout except somewhere
 * to keep its key table. The world lends it exactly that one slot. If the
 * reconciler ever needs more than this, the public API was not sufficient to
 * describe a scene, which is worth finding out.
 * -------------------------------------------------------------------------*/
typedef struct EshiSceneState EshiSceneState;

EshiSceneState* eshi__scene_create(void);
void            eshi__scene_destroy(EshiSceneState* scene);

/** The world's scene slot, lazily populated on first use. Never NULL. */
EshiSceneState** eshi__world_scene(EshiWorld* w);

/** This step's collision events, borrowed. Valid until the next eshi_tick(). */
const EshiCollisionEvent* eshi__collisions(const EshiWorld* w, int32_t* out_count);

/** Everything a render backend needs for one frame. Any out-param may be NULL. */
void eshi__frame_params(EshiWorld* w,
                        EshiShaderFn* out_shader,
                        const void**  out_uniforms,
                        size_t*       out_uniform_size,
                        const char**  out_source_path,
                        int32_t*      out_width,
                        int32_t*      out_height);

/* ---------------------------------------------------------------------------
 * Backend interface
 *
 * One vtable per Kantei grade. The world holds an opaque handle and never
 * learns which backend it got, so adding Filament in Phase 1 means adding a
 * table here and nothing else.
 * -------------------------------------------------------------------------*/
typedef struct EshiBackend EshiBackend;
typedef struct EshiBackendVTable EshiBackendVTable;

struct EshiBackendVTable {
    const char* name;

    /** Probes whether this backend can run right now. */
    int (*available)(void);

    /**
     * Creates backend state. A backend consumes either runtime `source_path`,
     * offline `package_path`, or neither for a compiled-in CPU shader.
     * Returns NULL on failure after writing a reason to stderr.
     */
    EshiBackend* (*create)(int32_t width, int32_t height,
                           const char* source_path, const char* package_path);

    void (*destroy)(EshiBackend* backend);

    /** Renders one frame. `uniforms` is the game's opaque block. */
    EshiResult (*render)(EshiBackend* backend,
                         uint8_t* pixels, int32_t stride, float time,
                         EshiShaderFn cpu_shader,
                         const void* uniforms, size_t uniform_size);

    /** Renders directly into a borrowed host Metal texture, when supported. */
    EshiResult (*render_texture)(EshiBackend* backend,
                                 void* metal_texture,
                                 void* pixel_buffer,
                                 int32_t width, int32_t height,
                                 float time,
                                 const void* uniforms, size_t uniform_size);

    /*
     * 3D scene operations, and the first place the vtable stops being uniform.
     *
     * A backend that has no scene leaves these NULL and the world answers
     * ESHI_ERR_UNSUPPORTED — which is the honest answer for Ink, Paper, and
     * direct Metal, all of which render a fullscreen material and nothing
     * else. Making them optional keeps the capability ladder in the vtable
     * rather than in a grade comparison somewhere else.
     */

    /** Parses and uploads a glTF/GLB. Writes a nonzero handle on success. */
    EshiResult (*asset_load)(EshiBackend* backend, const char* path,
                             uint32_t* out_asset);

    /** Adds one instance of a loaded asset to the backend's scene. */
    EshiResult (*asset_instance)(EshiBackend* backend, uint32_t asset,
                                 float x, float y, float z, float scale,
                                 float radians_per_second);

    /** Releases an asset and its instances. */
    EshiResult (*asset_release)(EshiBackend* backend, uint32_t asset);

    /** Assets currently loaded, for hosts and tests that ask. */
    uint32_t (*asset_count)(const EshiBackend* backend);
};

/**
 * Borrows the world's selected backend. Host adapters use this only to route
 * an opaque platform render target to the matching private backend entry
 * point; neither value is part of the installed C ABI.
 */
EshiBackend* eshi__world_backend(EshiWorld* w,
                                 const EshiBackendVTable** out_vtable);

/* Implemented per backend; the unavailable ones compile to stubs. */
const EshiBackendVTable* eshi__backend_ink(void);
const EshiBackendVTable* eshi__backend_gl(void);
const EshiBackendVTable* eshi__backend_metal(void);
const EshiBackendVTable* eshi__backend_filament(void);

/**
 * Renders into a borrowed macOS surface. Direct Metal uses `metal_texture`;
 * Filament uses `pixel_buffer` as an Apple CVPixelBuffer swapchain.
 *
 * This symbol is intentionally absent from eshi.h: it is consumed only by the
 * macOS Flutter host. The caller retains the texture until the synchronous
 * submission completes.
 */
EshiResult eshi__metal_render_texture(EshiWorld* w,
                                      void* metal_texture,
                                      void* pixel_buffer,
                                      int32_t width,
                                      int32_t height,
                                      float time);

/** Maps a grade to its table, or NULL if the grade has no backend here. */
const EshiBackendVTable* eshi__backend_for_grade(EshiGrade grade);

/** Set by eshi_gl_set_proc_loader(); read by the GL backend. */
EshiGlProcLoader eshi__gl_proc_loader(void);

#ifdef __cplusplus
}
#endif

#endif /* ESHI_INTERNAL_H */
