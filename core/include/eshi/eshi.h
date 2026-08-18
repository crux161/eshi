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

/*
 * Native and packed-stream compatibility are versioned independently. A Dart
 * package checks these constants against the loaded library before it creates
 * a world or maps either shared buffer.
 *
 * ABI versions use major.minor.patch packed into 8 bits each. A library accepts
 * the same major at an equal-or-newer minor; patch releases do not change the
 * contract. Packed protocols require an exact version match.
 */
#define ESHI_ABI_VERSION_PACK(major, minor, patch) \
    ((uint32_t)((((uint32_t)(major) & 0xFFu) << 24) | \
                (((uint32_t)(minor) & 0xFFu) << 16) | \
                (((uint32_t)(patch) & 0xFFu) << 8)))
#define ESHI_ABI_VERSION_MAJOR_OF(version) (((uint32_t)(version) >> 24) & 0xFFu)
#define ESHI_ABI_VERSION_MINOR_OF(version) (((uint32_t)(version) >> 16) & 0xFFu)
#define ESHI_ABI_VERSION_PATCH_OF(version) (((uint32_t)(version) >> 8) & 0xFFu)

#define ESHI_ABI_VERSION              ESHI_ABI_VERSION_PACK(1, 1, 0)
#define ESHI_COMMAND_PROTOCOL_VERSION 1u
#define ESHI_EVENT_PROTOCOL_VERSION   1u

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
    ESHI_ERR_LIMIT       = -4, /**< A fixed capacity was exhausted. */
    ESHI_ERR_STALE       = -5, /**< A scene older than the world's epoch; ignored. */
    ESHI_ERR_VERSION     = -6  /**< ABI or packed-protocol versions are incompatible. */
} EshiResult;

const char* eshi_result_string(EshiResult r);
const char* eshi_grade_string(EshiGrade g);

/* ===========================================================================
 * Native contract discovery
 *
 * EshiAbiInfo is append-only for an ABI major version. Every field is a u32 so
 * even a binding generator that cannot model platform alignment can inspect it
 * before touching one of the pointer-bearing public structures below.
 * ==========================================================================*/
typedef struct EshiAbiInfo {
    uint32_t struct_size;
    uint32_t abi_info_alignment;
    uint32_t abi_version;
    uint32_t command_protocol_version;
    uint32_t event_protocol_version;

    uint32_t pointer_size;
    uint32_t size_t_size;
    uint32_t function_pointer_size;
    uint32_t grade_size;
    uint32_t result_size;
    uint32_t entity_size;

    uint32_t config_size;
    uint32_t config_alignment;
    uint32_t config_width_offset;
    uint32_t config_height_offset;
    uint32_t config_grade_offset;
    uint32_t config_fixed_dt_offset;
    uint32_t config_seed_offset;
    uint32_t config_max_entities_offset;

    uint32_t transform_view_size;
    uint32_t transform_view_alignment;
    uint32_t transform_view_entity_offset;
    uint32_t transform_view_x_offset;
    uint32_t transform_view_y_offset;
    uint32_t transform_view_count_offset;

    uint32_t velocity_view_size;
    uint32_t velocity_view_alignment;
    uint32_t velocity_view_entity_offset;
    uint32_t velocity_view_vx_offset;
    uint32_t velocity_view_vy_offset;
    uint32_t velocity_view_count_offset;

    uint32_t collision_event_size;
    uint32_t collision_event_alignment;
    uint32_t collision_event_a_offset;
    uint32_t collision_event_b_offset;
    uint32_t collision_event_nx_offset;
    uint32_t collision_event_ny_offset;
    uint32_t collision_event_penetration_offset;

    uint32_t material_size;
    uint32_t material_alignment;
    uint32_t material_cpu_shader_offset;
    uint32_t material_source_path_offset;
    uint32_t material_package_path_offset;
    uint32_t material_uniform_data_offset;
    uint32_t material_uniform_size_offset;
} EshiAbiInfo;

/**
 * Rejects a generated binding before it touches native memory.
 *
 * The ABI major must match and this library's minor must be at least the
 * requested minor. Command and event protocols must match exactly.
 */
EshiResult eshi_abi_check(uint32_t requested_abi_version,
                          uint32_t requested_command_protocol,
                          uint32_t requested_event_protocol);

/**
 * Reports the concrete layout compiled into the loaded library.
 *
 * `out_size` is the caller's sizeof(EshiAbiInfo). The function copies only the
 * smaller of that size and the native structure, and returns ESHI_ERR_LIMIT if
 * the caller's structure is too small. At least four writable bytes are
 * required; `struct_size` then tells an older caller how much storage it needs.
 */
EshiResult eshi_abi_query(EshiAbiInfo* out_info, uint32_t out_size);

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
 *
 * A successful create owns exactly one world and must be paired with destroy;
 * destroying NULL is safe. A world and every borrowed pointer obtained from it
 * are thread-affine: call them only from the thread that created the world.
 * Independent worlds may be used concurrently on independent threads. The
 * diagnostic and grade-probe functions above do not require a world.
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
 * Command buffer — one boundary crossing per frame
 *
 * A dart:ffi leaf call is cheap but not free, and thousands of them per frame
 * plus the attendant GC pressure is exactly the overhead the data-oriented
 * layout exists to remove. So the boundary is crossed in bulk: the world owns
 * one buffer of 32-bit words, the caller maps it once — Dart wraps the same
 * bytes in an Int32List and a Float32List through asTypedList — writes packed
 * commands into it, and calls eshi_commands_flush() once. Events travel the
 * same way in reverse. Bulk in, bulk out.
 *
 * Wire format. Every command is one header word followed by its payload:
 *
 *     [ opcode:16 | payload_words:16 ][ payload_0 ] ... [ payload_n-1 ]
 *
 * The length in the header is what lets the decoder bounds-check a buffer
 * written by another language, and what lets a newer writer append payload
 * words an older reader ignores. It is deliberately *not* a licence to skip
 * unknown opcodes: an opcode this core does not implement stops the flush with
 * ESHI_ERR_UNSUPPORTED, because a Dart package that silently drops half a
 * scene is worse than one that refuses to run against a mismatched core.
 *
 * Float payloads are IEEE-754 bit patterns carried in a word.
 * ==========================================================================*/
typedef enum EshiCommand {
    ESHI_CMD_NOP         = 0, /**< No payload. Padding. */
    ESHI_CMD_SCENE_BEGIN = 1, /**< u32 epoch. Opens a reconcile pass. */
    ESHI_CMD_NODE        = 2, /**< u32 key, non-zero. Selects/creates a node. */
    ESHI_CMD_TRANSFORM   = 3, /**< f32 x, f32 y. */
    ESHI_CMD_VELOCITY    = 4, /**< f32 vx, f32 vy. */
    ESHI_CMD_COLLIDER    = 5, /**< f32 hx, hy; u32 layer, mask, flags. */
    ESHI_CMD_RESTITUTION = 6, /**< f32 restitution. */
    ESHI_CMD_BOUNDS      = 7, /**< f32 min_x, max_x, min_y, max_y. */
    ESHI_CMD_SCENE_END   = 8, /**< No payload. Runs the sweep. */
    ESHI_CMD_INPUT       = 9  /**< u32 EshiKey, u32 down. */
} EshiCommand;

#define ESHI_CMD_HEADER(op, words) \
    ((uint32_t)(((uint32_t)(op) & 0xFFFFu) | (((uint32_t)(words) & 0xFFFFu) << 16)))
#define ESHI_CMD_OP(header)    ((uint32_t)((header) & 0xFFFFu))
#define ESHI_CMD_WORDS(header) ((uint32_t)((header) >> 16))
#define ESHI_MAX_WIRE_PAYLOAD_WORDS  0xFFFFu
#define ESHI_MAX_SHARED_BUFFER_WORDS (1u << 24) /**< 64 MiB of u32 storage. */

/**
 * Grows the shared command buffer to at least `words` words.
 *
 * Invalidates every pointer previously returned by eshi_commands_data(), so a
 * mapped typed view must be rebuilt after this call. Callers that size the
 * buffer once at startup never see that happen. Requests above
 * ESHI_MAX_SHARED_BUFFER_WORDS return ESHI_ERR_LIMIT; allocation failures
 * return ESHI_ERR_NOMEM.
 */
EshiResult eshi_commands_reserve(EshiWorld* w, uint32_t words);

/** Base of the shared command buffer, or NULL before the first reserve. */
uint32_t* eshi_commands_data(EshiWorld* w);
uint32_t  eshi_commands_capacity(const EshiWorld* w);

/**
 * Decodes the first `word_count` words of the shared buffer.
 *
 * @param out_applied  Optional. Commands successfully applied, which on an
 *                     error is where the decoder stopped.
 * @return ESHI_OK, ESHI_ERR_STALE if a scene block was older than the world's
 *         epoch and was ignored, or an error at the first bad command.
 */
EshiResult eshi_commands_flush(EshiWorld* w, uint32_t word_count, uint32_t* out_applied);

/**
 * The same decoder over caller-owned memory.
 *
 * eshi_commands_flush() is this call against the shared buffer. A C++ host that
 * already has its own storage can skip the shared buffer entirely; Dart wants
 * the shared one, because that is the copy that never crosses the boundary.
 */
EshiResult eshi_commands_submit(EshiWorld* w, const uint32_t* words,
                                uint32_t word_count, uint32_t* out_applied);

/* ===========================================================================
 * Scene reconciler — retained, declarative, and safe to re-submit
 *
 * Dart's hot reload re-executes build(); it does not unwind native state. A
 * scene built by calling eshi_entity_create() from initState() therefore leaves
 * the old entities alive and creates a second set, and every reload doubles the
 * scene. The fix is not to make reload smarter, it is to stop describing the
 * scene imperatively.
 *
 * So a submission is a *description*: a list of nodes carrying stable keys. The
 * reconciler diffs it against the live ECS and emits the create/update/destroy
 * ops that close the gap — Flutter's own Widget -> Element -> RenderObject
 * model, applied across FFI. Submitting the same description twice is a no-op;
 * that property is what makes reload safe, and it is asserted in the tests.
 *
 * Two consequences worth stating outright:
 *
 *   - A component is written only when its described value *changed*. A reload
 *     that edits paddle speed moves the paddle and leaves the ball mid-flight,
 *     rather than teleporting the whole scene back to its spawn state. Systems
 *     stay the owner of simulation; the description owns only what was declared.
 *   - Dropping a node from the description destroys its entity, but dropping a
 *     component from a node does not remove that component — the core has no
 *     per-component removal yet. Drop the node instead.
 *
 * Epoch gating. A submission carries an epoch, and a scene older than the one
 * the world has already accepted is ignored with ESHI_ERR_STALE rather than
 * applied. That is what stops a closure left over from before a reload from
 * sweeping away the scene the new code just built. The gate covers scene
 * topology only: ESHI_CMD_INPUT is not part of the description and always
 * applies.
 *
 * A scene may span several flushes; the sweep runs on ESHI_CMD_SCENE_END and
 * nowhere else. A submission truncated before its end therefore leaves the
 * live scene untouched instead of deleting whatever did not arrive.
 * ==========================================================================*/

/** The entity reconciled for `key`, or ESHI_NULL_ENTITY if there is none. */
EshiEntity eshi_scene_entity(const EshiWorld* w, uint32_t key);

/** Highest scene epoch this world has accepted. */
uint32_t eshi_scene_epoch(const EshiWorld* w);

/** Nodes currently retained by the reconciler. */
uint32_t eshi_scene_node_count(const EshiWorld* w);

/**
 * Destroys every reconciled entity and forgets every key.
 *
 * The epoch is deliberately *not* rewound: unmounting a scene must not reopen
 * the gate to a stale writer that is still holding a buffer.
 */
void eshi_scene_clear(EshiWorld* w);

/* ===========================================================================
 * Event buffer — the same trick in reverse
 *
 * Same framing as commands, so one codec serves both directions.
 * ==========================================================================*/
typedef enum EshiEventType {
    ESHI_EVENT_COLLISION = 1 /**< u32 a, u32 b; f32 nx, ny, penetration. */
} EshiEventType;

/** Event buffers use the same limit and allocation error rules as commands. */
EshiResult      eshi_events_reserve(EshiWorld* w, uint32_t words);
const uint32_t* eshi_events_data(const EshiWorld* w);
uint32_t        eshi_events_capacity(const EshiWorld* w);

/**
 * Packs this step's events into the shared event buffer.
 *
 * @param out_words  Optional. Words written, always a whole number of records.
 * @return ESHI_OK, or ESHI_ERR_LIMIT when the buffer held only some of them —
 *         in which case the caller should grow it with eshi_events_reserve()
 *         rather than assume it saw everything. Unlike eshi_collisions_poll(),
 *         which silently truncates at `max`, overflow here is reported.
 */
EshiResult eshi_events_pack(EshiWorld* w, uint32_t* out_words);

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

/* ===========================================================================
 * Assets
 *
 * A 3D asset is an opaque handle. The loader, the meshes, the materials and
 * the renderable entities behind it belong to whichever backend the world
 * selected, and none of those types appear here — the boundary rule in
 * ARCHITECTURE.md §5 applies to Filament exactly as it does to SDL.
 *
 * Geometry is a capability, not a guarantee. A grade whose backend has no
 * scene — Ink, Paper, direct Metal — returns ESHI_ERR_UNSUPPORTED rather than
 * pretending to load something, so a host can ask instead of assuming.
 * ==========================================================================*/

/** An asset the backend has loaded. Zero is "no asset". */
typedef uint32_t EshiAsset;

/**
 * Loads a glTF or GLB file for this world's backend.
 *
 * Loading is separate from instancing because the expensive half — parsing,
 * decoding buffers, uploading vertex data and textures — happens once per
 * asset no matter how many copies of it a scene holds, and no matter how many
 * views draw that scene.
 *
 * Returns ESHI_ERR_UNSUPPORTED when the grade has no 3D scene,
 * ESHI_ERR_INVALID when the file is missing or is not a glTF the loader
 * accepts, and ESHI_ERR_NOMEM when the upload fails. A failed load leaves
 * `out_asset` untouched and the world renderable; it never half-loads.
 */
EshiResult eshi_asset_load(EshiWorld* w, const char* path, EshiAsset* out_asset);

/**
 * Places an instance of a loaded asset into the world's scene.
 *
 * Instances share the asset's uploaded geometry and materials; the cost of a
 * second one is a transform and a set of renderable entities, not a second
 * copy of the mesh.
 *
 * `x`, `y`, `z` position the instance, and `scale` is uniform. The convention
 * is right-handed, Y up, metres, matching glTF's own.
 */
EshiResult eshi_asset_instance(EshiWorld* w, EshiAsset asset,
                               float x, float y, float z, float scale);

/**
 * Places an instance whose root rotates around glTF's Y-up axis in the native
 * render system. The host declares the rate once; no per-frame FFI update is
 * required. `radians_per_second` may be negative and zero is stationary.
 */
EshiResult eshi_asset_instance_animated(EshiWorld* w, EshiAsset asset,
                                        float x, float y, float z, float scale,
                                        float radians_per_second);

/**
 * Releases an asset and every instance of it.
 *
 * Assets are also released with the world, so a host that exits does not need
 * to call this; a host that swaps scenes does.
 */
EshiResult eshi_asset_release(EshiWorld* w, EshiAsset asset);

/** Number of assets this world currently holds. Zero on grades without 3D. */
uint32_t eshi_asset_count(const EshiWorld* w);

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
