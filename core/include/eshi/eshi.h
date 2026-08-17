/**
 * @file eshi.h
 * @brief Larimar core — the entire public surface of the Eshi engine.
 *
 * This header is pure C99 on purpose. One header is consumed by three
 * toolchains with no glue: C++ hosts include it directly, Zig reaches it via
 * @cImport, and Dart generates bindings from it with ffigen.
 *
 * Boundary rules, enforced by review:
 *   - Nothing here names a windowing system, a graphics API, or an OS.
 *   - The core never opens a window, never polls an event queue, never owns
 *     a frame loop, and has no main(). Hosts do those things and call in.
 *   - No C++ types cross this boundary. No exceptions escape it.
 *
 * See docs/larimar/ARCHITECTURE.md.
 */
#ifndef ESHI_H
#define ESHI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESHI_VERSION_MAJOR 0
#define ESHI_VERSION_MINOR 1
#define ESHI_VERSION_PATCH 0

/* ===========================================================================
 * Kantei (判定) — hardware capability grades.
 *
 * The grade selects a render backend. Ink is below Filament's hardware floor
 * and remains reachable on CPU-only targets. Paper uses the lightweight GL
 * backend. Brush uses direct Metal by default and Filament when that optional
 * integration is built. Gold remains reserved for specialized hardware.
 * ==========================================================================*/
typedef enum EshiGrade {
    ESHI_GRADE_INK   = 1, /**< Pure CPU / software raster. ESP32, Playdate, server. */
    ESHI_GRADE_PAPER = 2, /**< GLES 2.0 / WebGL1 class hardware. */
    ESHI_GRADE_BRUSH = 3, /**< Metal / Vulkan / DX12 class hardware. */
    ESHI_GRADE_GOLD  = 4  /**< Ray tracing cores, neural accelerators. */
} EshiGrade;

typedef enum EshiResult {
    ESHI_OK              =  0,
    ESHI_ERR_INVALID     = -1, /**< Bad argument or dead entity. */
    ESHI_ERR_UNSUPPORTED = -2, /**< Grade or feature not built in this binary. */
    ESHI_ERR_NOMEM       = -3,
    ESHI_ERR_LIMIT       = -4  /**< A fixed capacity was exhausted. */
} EshiResult;

const char* eshi_result_string(EshiResult r);
const char* eshi_grade_string(EshiGrade g);

/* ===========================================================================
 * Entities
 *
 * 32-bit storage: 17-bit index + 8-bit generation. The generation makes a stale
 * handle detectable rather than silently aliasing a recycled slot. Width and
 * bit layout match Filament's utils::Entity in the vendored release so the
 * render bridge can adopt it as the shared id without a translation table.
 * ==========================================================================*/
typedef uint32_t EshiEntity;

#define ESHI_NULL_ENTITY   ((EshiEntity)0)
#define ESHI_ENTITY_INDEX_BITS       17u
#define ESHI_ENTITY_GENERATION_BITS  8u
#define ESHI_MAX_ENTITIES            ((uint32_t)0x0001FFFFu)

#define ESHI_ENTITY_INDEX(e)      ((uint32_t)((e) & ESHI_MAX_ENTITIES))
#define ESHI_ENTITY_GENERATION(e) \
    ((uint32_t)(((e) >> ESHI_ENTITY_INDEX_BITS) & 0xFFu))

/* ===========================================================================
 * World lifecycle
 * ==========================================================================*/
typedef struct EshiWorld EshiWorld;

typedef struct EshiConfig {
    int32_t   width;         /**< Framebuffer width in pixels. */
    int32_t   height;        /**< Framebuffer height in pixels. */
    EshiGrade grade;         /**< Requested backend tier. */
    float     fixed_dt;      /**< Simulation step. 0 selects 1/60. */
    uint64_t  seed;          /**< RNG seed. Equal seeds produce equal frames. */
    uint32_t  max_entities;  /**< 0 selects 4096. */
} EshiConfig;

/** Returns a config with every field at its documented default. */
EshiConfig eshi_config_default(void);

EshiWorld* eshi_world_create(const EshiConfig* cfg);
void       eshi_world_destroy(EshiWorld* w);

EshiGrade eshi_world_grade(const EshiWorld* w);
/** Human-readable implementation selected for this world (for diagnostics). */
const char* eshi_world_backend_name(const EshiWorld* w);
void      eshi_world_size(const EshiWorld* w, int32_t* out_w, int32_t* out_h);

/* ===========================================================================
 * Entity lifecycle
 * ==========================================================================*/
EshiEntity eshi_entity_create(EshiWorld* w);
void       eshi_entity_destroy(EshiWorld* w, EshiEntity e);
int        eshi_entity_alive(const EshiWorld* w, EshiEntity e);
uint32_t   eshi_entity_count(const EshiWorld* w);

/* ===========================================================================
 * Components — plain data, no logic, stored structure-of-arrays.
 *
 * The per-entity setters below are the convenience path, intended for scene
 * construction. Systems that run every frame should take a view (further
 * down) and walk the arrays instead; that is the whole point of the layout.
 * ==========================================================================*/

/** Position in world units. Scale is the collider-independent visual size. */
EshiResult eshi_transform_set(EshiWorld* w, EshiEntity e, float x, float y);
EshiResult eshi_transform_get(const EshiWorld* w, EshiEntity e, float* x, float* y);

/** Linear velocity in world units per second. Integrated by the motion system. */
EshiResult eshi_velocity_set(EshiWorld* w, EshiEntity e, float vx, float vy);
EshiResult eshi_velocity_get(const EshiWorld* w, EshiEntity e, float* vx, float* vy);

/** Collider response flags. */
typedef enum EshiColliderFlags {
    ESHI_COLLIDER_NONE    = 0,
    ESHI_COLLIDER_TRIGGER = 1u << 0, /**< Report the overlap, do not resolve it. */
    ESHI_COLLIDER_STATIC  = 1u << 1  /**< Never displaced by a resolution. */
} EshiColliderFlags;

/**
 * Axis-aligned box collider.
 *
 * @param hx,hy   Half extents.
 * @param layer   Bitmask identifying what this entity *is*.
 * @param mask    Bitmask identifying what this entity *collides with*.
 *                A pair interacts when (a.layer & b.mask) && (b.layer & a.mask).
 * @param flags   EshiColliderFlags.
 */
EshiResult eshi_collider_set(EshiWorld* w, EshiEntity e,
                             float hx, float hy,
                             uint32_t layer, uint32_t mask,
                             uint32_t flags);

/** Restitution applied on resolve. 1.0 is a perfect bounce, 0.0 absorbs. */
EshiResult eshi_collider_set_restitution(EshiWorld* w, EshiEntity e, float restitution);

/**
 * Clamps the transform to an axis-aligned range after motion integrates.
 * Generic replacement for the hand-written paddle clamping in the old Pong.
 */
EshiResult eshi_bounds_set(EshiWorld* w, EshiEntity e,
                           float min_x, float max_x,
                           float min_y, float max_y);

/* ===========================================================================
 * Structure-of-arrays views
 *
 * Views are borrowed pointers into dense component storage, valid until the
 * next call that adds or removes a component of that type. Parallel arrays;
 * index i of every array in a view refers to the same entity.
 * ==========================================================================*/
typedef struct EshiTransformView {
    const EshiEntity* entity;
    float*            x;
    float*            y;
    int32_t           count;
} EshiTransformView;

typedef struct EshiVelocityView {
    const EshiEntity* entity;
    float*            vx;
    float*            vy;
    int32_t           count;
} EshiVelocityView;

EshiTransformView eshi_view_transforms(EshiWorld* w);
EshiVelocityView  eshi_view_velocities(EshiWorld* w);

/* ===========================================================================
 * Systems — pure functions over component arrays.
 *
 * Systems run in ascending `order`, and ties break on registration sequence.
 * The ordering is stable across runs, which is one of the three things the
 * deterministic video export depends on (the others being the fixed timestep
 * and the seeded RNG).
 * ==========================================================================*/
typedef void (*EshiSystemFn)(EshiWorld* w, float dt, void* user);

/** Built-in system orders. Game systems should sit between these. */
#define ESHI_ORDER_INPUT      (-1000)
#define ESHI_ORDER_GAMEPLAY   (0)
#define ESHI_ORDER_MOTION     (1000)   /**< Integrates velocity into transform. */
#define ESHI_ORDER_COLLISION  (2000)   /**< Detects and resolves, emits events. */
#define ESHI_ORDER_PRESENT    (3000)   /**< Packs component state into uniforms. */

EshiResult eshi_system_add(EshiWorld* w, const char* name, int32_t order,
                           EshiSystemFn fn, void* user);

/* ===========================================================================
 * Collision events
 *
 * Drained by the game, not pushed to it. The engine resolves the geometry
 * generically; game-specific response (spin, speed-up, scoring, audio) reads
 * these events and reacts. There is no ResolvePaddleBounce().
 * ==========================================================================*/
typedef struct EshiCollisionEvent {
    EshiEntity a;
    EshiEntity b;
    float      nx;         /**< Contact normal, pointing from b toward a. */
    float      ny;
    float      penetration;
} EshiCollisionEvent;

/** Copies up to `max` events into `out`. Returns the number written. */
int32_t eshi_collisions_poll(EshiWorld* w, EshiCollisionEvent* out, int32_t max);

/* ===========================================================================
 * Input — abstract keys only.
 *
 * The core has no idea SDL exists. Hosts translate their native scancodes to
 * these and push them in before eshi_tick(). This is what lets one game source
 * run unmodified under both the SDL host and the Flutter embedder.
 * ==========================================================================*/
typedef enum EshiKey {
    ESHI_KEY_UP = 0,
    ESHI_KEY_DOWN,
    ESHI_KEY_LEFT,
    ESHI_KEY_RIGHT,
    ESHI_KEY_W,
    ESHI_KEY_A,
    ESHI_KEY_S,
    ESHI_KEY_D,
    ESHI_KEY_SPACE,
    ESHI_KEY_ESCAPE,
    ESHI_KEY_COUNT
} EshiKey;

void eshi_input_set_key(EshiWorld* w, EshiKey key, int down);
int  eshi_input_down(const EshiWorld* w, EshiKey key);
int  eshi_input_pressed(const EshiWorld* w, EshiKey key);   /**< Edge: up -> down. */
int  eshi_input_released(const EshiWorld* w, EshiKey key);  /**< Edge: down -> up. */

/* ===========================================================================
 * Material — the fullscreen shader and its uniform block.
 *
 * This is the fix for the defect that made feature/pong-engine unmaintainable:
 * there, adding a game changed renderFrame() on every backend and put a
 * game-specific struct into the shared math header. Here the uniform block is
 * opaque to the engine. The game declares its own layout, the engine only
 * forwards the pointer, and no backend signature mentions any game.
 *
 * On Ink the block is passed straight to the shader function. On Filament it
 * becomes a material parameter buffer upload; the game-facing contract does
 * not change.
 * ==========================================================================*/

/**
 * Fragment entry point.
 *
 * @param out_rgba   Four floats, unclamped linear RGBA, written by the shader.
 * @param frag_x     Pixel centre, origin bottom-left, matching Shadertoy.
 * @param frag_y
 * @param res_x      Framebuffer resolution.
 * @param res_y
 * @param time       Seconds. Derived from the frame index in headless renders.
 * @param uniforms   The game's block, or NULL if none was set.
 */
typedef void (*EshiShaderFn)(float* out_rgba,
                             float frag_x, float frag_y,
                             float res_x, float res_y,
                             float time,
                             const void* uniforms);

/**
 * A material, described once for every tier.
 *
 * Ink executes `cpu_shader` directly — it is compiled into the binary. The GPU
 * tiers cannot call a host function pointer. Lightweight GPU backends take
 * `source_path` and transpile it at runtime; packaged renderers take a material
 * package produced by the offline toolchain. Supplying all representations is
 * what lets one material run on every grade; omitting one restricts it to the
 * remaining tiers.
 *
 * Source-backed tiers get shader hot-reload by rebinding after a file change.
 * Package-backed tiers rebind a rebuilt package; Ink remains compiled into the
 * host binary. The representation changes, but the material API does not.
 */
typedef struct EshiMaterial {
    EshiShaderFn cpu_shader;   /**< Ink. NULL restricts the material to GPU tiers. */
    const char*  source_path;  /**< Runtime-transpiled source, or NULL. */
    const char*  package_path; /**< Offline-compiled material package, or NULL. */
    void*        uniform_data; /**< Caller-owned block, or NULL. */
    size_t       uniform_size; /**< Bytes of `uniform_data`. */
} EshiMaterial;

/**
 * Binds the fullscreen material.
 *
 * `uniform_data` stays owned by the caller and must outlive the world. It is
 * read once per frame at render time, so a system at ESHI_ORDER_PRESENT can
 * repack it from component arrays each tick. On the GPU tiers the block is
 * uploaded as a flat float array named `eshi_uniforms`.
 */
EshiResult eshi_material_set(EshiWorld* w, const EshiMaterial* material);

/**
 * Reports whether a grade is compiled into this binary and usable right now.
 *
 * Hosts probe with this and pick; eshi_world_create() never silently downgrades
 * a caller who asked for hardware they expected to have.
 *
 * Paper (OpenGL) additionally requires eshi_gl_set_proc_loader() to have been
 * called and a context to be current, so this returns 0 for Paper until then.
 */
int eshi_grade_available(EshiGrade grade);

/** Highest grade available in this binary, or ESHI_GRADE_INK. */
EshiGrade eshi_grade_best(void);

/**
 * Supplies OpenGL entry points for the Paper backend.
 *
 * The core links no windowing library, so it cannot resolve GL symbols itself.
 * The host — which already owns the context — passes its loader (SDL_GL_GetProcAddress
 * or equivalent) and guarantees a current context on every eshi_render() call.
 * This is the boundary rule from ARCHITECTURE.md §5 applied to OpenGL.
 */
typedef void* (*EshiGlProcLoader)(const char* name);
void eshi_gl_set_proc_loader(EshiGlProcLoader loader);

/* ===========================================================================
 * Frame
 *
 * Pull-driven. The core never owns a loop: under SDL the host drives, and
 * under Flutter the embedder's vsync callback drives. Calling eshi_tick from
 * a host callback must remain valid, so nothing here blocks or sleeps.
 * ==========================================================================*/

/**
 * Advances the simulation.
 *
 * `dt` is accumulated and consumed in fixed steps of config.fixed_dt, so the
 * number of system executions depends only on elapsed simulated time and never
 * on frame rate. Pass exactly fixed_dt from a headless renderer to make the
 * output byte-reproducible.
 */
void eshi_tick(EshiWorld* w, float dt);

/**
 * Renders one frame into a host-provided buffer.
 *
 * @param pixels  RGBA8, at least height*stride bytes.
 * @param stride  Bytes per row.
 * @param time    Seconds handed to the shader.
 */
EshiResult eshi_render(EshiWorld* w, uint8_t* pixels, int32_t stride, float time);

uint64_t eshi_frame_index(const EshiWorld* w);
double   eshi_sim_time(const EshiWorld* w);

/* ===========================================================================
 * Deterministic RNG
 *
 * Seeded per world. Systems must draw from here rather than rand() or a
 * wall-clock, or headless renders stop being reproducible.
 * ==========================================================================*/
uint32_t eshi_random_u32(EshiWorld* w);
float    eshi_random_float(EshiWorld* w); /**< Uniform in [0,1). */
float    eshi_random_range(EshiWorld* w, float lo, float hi);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ESHI_H */
