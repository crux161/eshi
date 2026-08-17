/**
 * @file test_core.cpp
 * @brief Phase 0 smoke tests for the Larimar core.
 *
 * Focused on the failure modes that are silent rather than loud: sparse-set
 * swap-and-pop leaving parallel arrays misaligned, recycled entity indices
 * resurrecting stale handles, and collision layers matching when they should
 * not. A wrong answer in any of those looks like a gameplay bug months later.
 */
#include <cmath>
#include <cstdio>
#include <cstring>

#include <eshi/eshi.h>
#include <eshi/scene.hpp>

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const char* what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("  FAIL  %s\n", what);
    }
}

void check_near(float actual, float expected, float tolerance, const char* what) {
    check(std::fabs(actual - expected) <= tolerance, what);
}

EshiWorld* make_world() {
    EshiConfig cfg = eshi_config_default();
    cfg.width = 64;
    cfg.height = 64;
    return eshi_world_create(&cfg);
}

/* ------------------------------------------------------------------------ */

void test_grade_gate() {
    std::printf("grade gate\n");

    /* Ink has no hardware requirement, so it is always available. */
    check(eshi_grade_available(ESHI_GRADE_INK) == 1, "Ink is always available");

    /* This target links no GPU backend, so the upper grades must say so. */
    check(eshi_grade_available(ESHI_GRADE_PAPER) == 0, "Paper unavailable without a GL loader");
    check(eshi_grade_available(ESHI_GRADE_BRUSH) == 0, "Brush unavailable in a core-only build");
    check(eshi_grade_available(ESHI_GRADE_GOLD) == 0, "Gold has no backend yet");
    check(eshi_grade_best() == ESHI_GRADE_INK, "best available grade falls to Ink");

    EshiConfig cfg = eshi_config_default();
    cfg.grade = ESHI_GRADE_BRUSH;
    check(eshi_world_create(&cfg) == NULL, "unavailable grade is refused, not downgraded");

    EshiWorld* w = make_world();
    check(w != NULL, "Ink grade is accepted");
    check(eshi_world_grade(w) == ESHI_GRADE_INK, "grade round-trips");
    check(std::strcmp(eshi_world_backend_name(w), "ink") == 0,
          "world reports the selected backend");
    eshi_world_destroy(w);
}

/* A shader that paints a fixed colour, so render output is predictable. */
void flat_shader(float* out_rgba, float, float, float, float, float, const void* uniforms) {
    const float value = uniforms ? *static_cast<const float*>(uniforms) : 0.0f;
    out_rgba[0] = value;
    out_rgba[1] = 0.0f;
    out_rgba[2] = 0.0f;
    out_rgba[3] = 1.0f;
}

void test_material_and_render() {
    std::printf("material and render\n");
    EshiWorld* w = make_world();

    uint8_t pixels[64 * 64 * 4];
    check(eshi_render(w, pixels, 64 * 4, 0.0f) == ESHI_ERR_INVALID,
          "render without a material is refused");

    EshiMaterial empty;
    empty.cpu_shader = NULL;
    empty.source_path = NULL;
    empty.package_path = NULL;
    empty.uniform_data = NULL;
    empty.uniform_size = 0;
    check(eshi_material_set(w, &empty) == ESHI_ERR_INVALID,
          "a material with neither entry point is refused");

    float red = 1.0f;
    EshiMaterial material;
    material.cpu_shader = flat_shader;
    material.source_path = NULL;
    material.package_path = NULL;
    material.uniform_data = &red;
    material.uniform_size = sizeof(red);
    check(eshi_material_set(w, &material) == ESHI_OK, "Ink accepts a cpu_shader material");

    check(eshi_render(w, pixels, 64 * 4, 0.0f) == ESHI_OK, "render succeeds");
    check(pixels[0] == 255, "shader output reaches the framebuffer");
    check(pixels[1] == 0, "untouched channel stays zero");
    check(pixels[3] == 255, "alpha is written");

    /* The uniform block is read every frame, not captured at bind time. */
    red = 0.0f;
    eshi_render(w, pixels, 64 * 4, 0.0f);
    check(pixels[0] == 0, "uniform block is re-read each frame");

    eshi_world_destroy(w);
}

void test_entity_generations() {
    std::printf("entity generations\n");
    EshiWorld* w = make_world();

    EshiEntity a = eshi_entity_create(w);
    check(a != ESHI_NULL_ENTITY, "created entity is non-null");
    check(eshi_entity_alive(w, a) == 1, "created entity is alive");
    check(eshi_entity_count(w) == 1, "live count tracks creation");

    eshi_entity_destroy(w, a);
    check(eshi_entity_alive(w, a) == 0, "destroyed handle is dead");
    check(eshi_entity_count(w) == 0, "live count tracks destruction");

    /* The index is recycled; the stale handle must not come back to life. */
    EshiEntity b = eshi_entity_create(w);
    check(ESHI_ENTITY_INDEX(b) == ESHI_ENTITY_INDEX(a), "index is recycled");
    check(b != a, "recycled handle differs from the stale one");
    check(eshi_entity_alive(w, a) == 0, "stale handle stays dead after recycle");
    check(eshi_entity_alive(w, b) == 1, "fresh handle is alive");

    check(eshi_transform_set(w, a, 1.0f, 1.0f) == ESHI_ERR_INVALID,
          "stale handle is rejected by component setters");

    eshi_world_destroy(w);
}

void test_sparse_set_removal() {
    std::printf("sparse set swap-and-pop\n");
    EshiWorld* w = make_world();

    /* Three entities with distinct transforms, then remove the middle one. */
    EshiEntity e[3];
    for (int i = 0; i < 3; ++i) {
        e[i] = eshi_entity_create(w);
        eshi_transform_set(w, e[i], (float)i, (float)(i * 10));
    }

    eshi_entity_destroy(w, e[1]);

    float x = 0.0f, y = 0.0f;
    check(eshi_transform_get(w, e[0], &x, &y) == ESHI_OK, "first survivor readable");
    check_near(x, 0.0f, 1e-6f, "first survivor x intact");
    check_near(y, 0.0f, 1e-6f, "first survivor y intact");

    check(eshi_transform_get(w, e[2], &x, &y) == ESHI_OK, "moved survivor readable");
    check_near(x, 2.0f, 1e-6f, "moved survivor x intact after swap-and-pop");
    check_near(y, 20.0f, 1e-6f, "moved survivor y intact after swap-and-pop");

    EshiTransformView view = eshi_view_transforms(w);
    check(view.count == 2, "dense array shrank by exactly one");

    eshi_world_destroy(w);
}

void test_motion_and_bounds() {
    std::printf("motion and bounds\n");
    EshiWorld* w = make_world();

    EshiEntity e = eshi_entity_create(w);
    eshi_transform_set(w, e, 0.0f, 0.0f);
    eshi_velocity_set(w, e, 1.0f, 0.0f);

    for (int i = 0; i < 60; ++i) eshi_tick(w, 1.0f / 60.0f);

    float x = 0.0f;
    eshi_transform_get(w, e, &x, NULL);
    check_near(x, 1.0f, 1e-3f, "one second at 1 unit/s travels 1 unit");
    check(eshi_frame_index(w) == 60, "fixed timestep ran exactly 60 steps");

    /* Bounds clamp after integration. */
    EshiEntity c = eshi_entity_create(w);
    eshi_transform_set(w, c, 0.0f, 0.0f);
    eshi_velocity_set(w, c, 0.0f, 10.0f);
    eshi_bounds_set(w, c, -1.0f, 1.0f, -0.5f, 0.5f);

    for (int i = 0; i < 60; ++i) eshi_tick(w, 1.0f / 60.0f);

    float y = 0.0f;
    eshi_transform_get(w, c, NULL, &y);
    check_near(y, 0.5f, 1e-4f, "bounds clamp holds against velocity");

    eshi_world_destroy(w);
}

void test_collision_layers() {
    std::printf("collision layers and events\n");
    const uint32_t kBall = 1u << 0;
    const uint32_t kWall = 1u << 1;
    const uint32_t kGhost = 1u << 2;

    EshiWorld* w = make_world();

    EshiEntity ball = eshi_entity_create(w);
    eshi_transform_set(w, ball, 0.0f, 0.0f);
    eshi_velocity_set(w, ball, 0.0f, 0.0f);
    eshi_collider_set(w, ball, 0.5f, 0.5f, kBall, kWall, ESHI_COLLIDER_NONE);

    /* Overlapping, but on a layer the ball does not mask against. */
    EshiEntity ghost = eshi_entity_create(w);
    eshi_transform_set(w, ghost, 0.0f, 0.0f);
    eshi_collider_set(w, ghost, 0.5f, 0.5f, kGhost, kGhost, ESHI_COLLIDER_STATIC);

    eshi_tick(w, 1.0f / 60.0f);

    EshiCollisionEvent events[8];
    check(eshi_collisions_poll(w, events, 8) == 0, "mismatched layers produce no event");

    /* Now an overlapping wall the ball does mask against. */
    EshiEntity wall = eshi_entity_create(w);
    eshi_transform_set(w, wall, 0.6f, 0.0f);
    eshi_collider_set(w, wall, 0.5f, 0.5f, kWall, kBall, ESHI_COLLIDER_STATIC);

    eshi_tick(w, 1.0f / 60.0f);

    const int32_t n = eshi_collisions_poll(w, events, 8);
    check(n == 1, "matched layers produce exactly one event");
    if (n == 1) {
        const bool pair_ok = (events[0].a == ball && events[0].b == wall) ||
                             (events[0].a == wall && events[0].b == ball);
        check(pair_ok, "event names the colliding pair");
        check(std::fabs(events[0].nx) > 0.5f, "normal resolves along the shallow axis");
        check(events[0].penetration > 0.0f, "penetration is positive");
    }

    /* The static wall must not have moved; the ball must have been pushed out. */
    float wall_x = 0.0f, ball_x = 0.0f;
    eshi_transform_get(w, wall, &wall_x, NULL);
    eshi_transform_get(w, ball, &ball_x, NULL);
    check_near(wall_x, 0.6f, 1e-5f, "static body is never displaced");
    check(ball_x < 0.0f, "dynamic body is pushed clear of the static one");

    eshi_world_destroy(w);
}

void test_restitution_reflects() {
    std::printf("restitution\n");
    EshiWorld* w = make_world();

    const uint32_t kA = 1u << 0, kB = 1u << 1;

    EshiEntity ball = eshi_entity_create(w);
    eshi_transform_set(w, ball, -0.4f, 0.0f);
    eshi_velocity_set(w, ball, 1.0f, 0.0f);
    eshi_collider_set(w, ball, 0.5f, 0.5f, kA, kB, ESHI_COLLIDER_NONE);
    eshi_collider_set_restitution(w, ball, 1.0f);

    EshiEntity wall = eshi_entity_create(w);
    eshi_transform_set(w, wall, 0.4f, 0.0f);
    eshi_collider_set(w, wall, 0.5f, 0.5f, kB, kA, ESHI_COLLIDER_STATIC);

    eshi_tick(w, 1.0f / 60.0f);

    float vx = 0.0f;
    eshi_velocity_get(w, ball, &vx, NULL);
    check(vx < 0.0f, "velocity reverses on a perfectly elastic hit");
    check_near(std::fabs(vx), 1.0f, 1e-4f, "elastic bounce preserves speed");

    eshi_world_destroy(w);
}

void test_determinism() {
    std::printf("determinism\n");

    float first_x = 0.0f, first_y = 0.0f;
    for (int run = 0; run < 2; ++run) {
        EshiConfig cfg = eshi_config_default();
        cfg.width = 64;
        cfg.height = 64;
        cfg.seed = 12345;
        EshiWorld* w = eshi_world_create(&cfg);

        EshiEntity e = eshi_entity_create(w);
        eshi_transform_set(w, e, 0.0f, 0.0f);
        for (int i = 0; i < 100; ++i) {
            eshi_velocity_set(w, e,
                              eshi_random_range(w, -1.0f, 1.0f),
                              eshi_random_range(w, -1.0f, 1.0f));
            eshi_tick(w, 1.0f / 60.0f);
        }

        float x = 0.0f, y = 0.0f;
        eshi_transform_get(w, e, &x, &y);
        if (run == 0) { first_x = x; first_y = y; }
        else {
            check(x == first_x && y == first_y, "same seed reproduces the same trajectory bit for bit");
        }
        eshi_world_destroy(w);
    }
}

void test_system_ordering() {
    std::printf("system ordering\n");
    static int order_log[4];
    static int order_count;
    order_count = 0;

    struct Fns {
        static void a(EshiWorld*, float, void*) { order_log[order_count++] = 1; }
        static void b(EshiWorld*, float, void*) { order_log[order_count++] = 2; }
        static void c(EshiWorld*, float, void*) { order_log[order_count++] = 3; }
    };

    EshiWorld* w = make_world();
    /* Registered out of order, and two share a rank to exercise the tie-break. */
    eshi_system_add(w, "c", 50, Fns::c, NULL);
    eshi_system_add(w, "a", 10, Fns::a, NULL);
    eshi_system_add(w, "b", 10, Fns::b, NULL);

    eshi_tick(w, 1.0f / 60.0f);

    check(order_count == 3, "every system ran once");
    check(order_log[0] == 1 && order_log[1] == 2 && order_log[2] == 3,
          "ascending order, ties broken by registration sequence");

    eshi_world_destroy(w);
}

void test_input_edges() {
    std::printf("input edges\n");
    EshiWorld* w = make_world();

    eshi_input_set_key(w, ESHI_KEY_SPACE, 1);
    eshi_tick(w, 1.0f / 60.0f);
    check(eshi_input_down(w, ESHI_KEY_SPACE) == 1, "held key reads down");

    eshi_tick(w, 1.0f / 60.0f);
    check(eshi_input_pressed(w, ESHI_KEY_SPACE) == 0, "held key is not re-pressed");

    eshi_input_set_key(w, ESHI_KEY_SPACE, 0);
    eshi_tick(w, 1.0f / 60.0f);
    check(eshi_input_down(w, ESHI_KEY_SPACE) == 0, "released key reads up");

    eshi_world_destroy(w);
}

/* ===========================================================================
 * Command buffer and scene reconciler
 *
 * These carry more weight than the rest of the file. Everything above fails
 * loudly when it breaks; the reconciler's failure mode is a Flutter app whose
 * scene quietly doubles on every hot reload, which looks like a memory leak
 * for a week before anyone counts the entities.
 * ==========================================================================*/

const uint32_t kKeyBall   = 101;
const uint32_t kKeyPaddle = 102;
const uint32_t kKeyWall   = 103;

/** The scene these tests reconcile against, parameterised by what may change. */
void describe(eshi::SceneWriter& scene, uint32_t epoch, float paddle_y, bool with_wall) {
    scene.begin(epoch);

    scene.node(kKeyBall)
        .transform(0.0f, 0.0f)
        .velocity(1.0f, 0.0f)
        .collider(0.05f, 0.05f, 1u, 2u, ESHI_COLLIDER_NONE)
        .restitution(1.0f);

    scene.node(kKeyPaddle)
        .transform(-1.0f, paddle_y)
        .collider(0.02f, 0.2f, 2u, 1u, ESHI_COLLIDER_STATIC)
        .bounds(-1.0f, -1.0f, -0.8f, 0.8f);

    if (with_wall) {
        scene.node(kKeyWall)
            .transform(0.0f, 1.0f)
            .collider(2.0f, 0.1f, 2u, 1u, ESHI_COLLIDER_STATIC);
    }

    scene.end();
}

void test_command_wire_format() {
    std::printf("command wire format\n");
    EshiWorld* w = make_world();

    const uint32_t header = ESHI_CMD_HEADER(ESHI_CMD_COLLIDER, 5);
    check(ESHI_CMD_OP(header) == (uint32_t)ESHI_CMD_COLLIDER, "opcode survives the header pack");
    check(ESHI_CMD_WORDS(header) == 5, "payload length survives the header pack");

    uint32_t applied = 0;

    /* A payload that runs off the end of the buffer must not be read. */
    const uint32_t truncated[2] = { ESHI_CMD_HEADER(ESHI_CMD_TRANSFORM, 2), 0u };
    check(eshi_commands_submit(w, truncated, 2, &applied) == ESHI_ERR_INVALID,
          "a payload longer than the buffer is rejected");
    check(applied == 0, "nothing is applied from a malformed buffer");

    /* Declaring fewer words than the opcode needs is equally malformed. */
    const uint32_t short_payload[2] = { ESHI_CMD_HEADER(ESHI_CMD_TRANSFORM, 1), 0u };
    check(eshi_commands_submit(w, short_payload, 2, &applied) == ESHI_ERR_INVALID,
          "a payload shorter than the opcode's arity is rejected");

    /*
     * An opcode this core does not implement stops the flush. The length field
     * is there to bounds-check and to tolerate *extra* payload, not to let a
     * mismatched writer silently lose half a scene.
     */
    const uint32_t unknown[1] = { ESHI_CMD_HEADER(4095, 0) };
    check(eshi_commands_submit(w, unknown, 1, &applied) == ESHI_ERR_UNSUPPORTED,
          "an unknown opcode is refused rather than skipped");

    /* Extra payload from a newer writer is ignored, not refused. */
    const uint32_t extended[5] = {
        ESHI_CMD_HEADER(ESHI_CMD_SCENE_BEGIN, 2), 7u, 0xDEADBEEFu,
        ESHI_CMD_HEADER(ESHI_CMD_SCENE_END, 0), 0u
    };
    check(eshi_commands_submit(w, extended, 4, &applied) == ESHI_OK,
          "a longer payload than this core reads is accepted");
    check(eshi_scene_epoch(w) == 7, "the known prefix of an extended payload still applies");

    /* Components need an open node; a stray one is a bug worth reporting. */
    const uint32_t orphan[3] = { ESHI_CMD_HEADER(ESHI_CMD_TRANSFORM, 2), 0u, 0u };
    check(eshi_commands_submit(w, orphan, 3, &applied) == ESHI_ERR_INVALID,
          "a component with no open node is rejected");

    eshi_world_destroy(w);
}

void test_shared_command_buffer() {
    std::printf("shared command buffer\n");
    EshiWorld* w = make_world();

    check(eshi_commands_data(w) == NULL, "no buffer is allocated until it is asked for");
    check(eshi_commands_reserve(w, 256) == ESHI_OK, "the buffer can be reserved");
    check(eshi_commands_capacity(w) >= 256, "capacity reflects the reservation");

    uint32_t* words = eshi_commands_data(w);
    check(words != NULL, "the reserved buffer is mappable");

    /* This is the shape Dart uses: write into the shared words, cross once. */
    eshi::SceneWriter scene(words, eshi_commands_capacity(w));
    describe(scene, 1, 0.0f, true);
    check(!scene.overflowed(), "the description fits the reserved buffer");

    uint32_t applied = 0;
    check(eshi_commands_flush(w, scene.size(), &applied) == ESHI_OK, "flush succeeds");
    check(applied > 0, "flush reports the commands it applied");
    check(eshi_scene_node_count(w) == 3, "three described nodes became three nodes");
    check(eshi_entity_count(w) == 3, "and three entities");

    check(eshi_commands_flush(w, eshi_commands_capacity(w) + 1, NULL) == ESHI_ERR_LIMIT,
          "flushing past the buffer's end is refused");

    eshi_world_destroy(w);
}

void test_scene_reconcile() {
    std::printf("scene reconcile\n");
    EshiWorld* w = make_world();

    uint32_t words[256];
    eshi::SceneWriter scene(words, 256);
    describe(scene, 1, 0.25f, true);
    check(scene.submit(w) == ESHI_OK, "a scene description is accepted");

    const EshiEntity ball = eshi_scene_entity(w, kKeyBall);
    check(ball != ESHI_NULL_ENTITY, "a key resolves to an entity");
    check(eshi_entity_alive(w, ball) == 1, "the reconciled entity is alive");
    check(eshi_scene_entity(w, 999) == ESHI_NULL_ENTITY, "an undescribed key resolves to null");
    check(eshi_scene_entity(w, 0) == ESHI_NULL_ENTITY, "key zero is never valid");
    check(eshi_scene_epoch(w) == 1, "the world records the accepted epoch");

    /* Every described component reached the ECS, not just the entity. */
    float x = 0.0f, y = 0.0f;
    check(eshi_transform_get(w, eshi_scene_entity(w, kKeyPaddle), &x, &y) == ESHI_OK,
          "a described transform exists");
    check_near(x, -1.0f, 1e-6f, "described x is applied");
    check_near(y, 0.25f, 1e-6f, "described y is applied");

    float vx = 0.0f;
    check(eshi_velocity_get(w, ball, &vx, NULL) == ESHI_OK, "a described velocity exists");
    check_near(vx, 1.0f, 1e-6f, "described velocity is applied");

    eshi_scene_clear(w);
    check(eshi_scene_node_count(w) == 0, "clear forgets every node");
    check(eshi_entity_count(w) == 0, "clear destroys every reconciled entity");
    check(eshi_scene_epoch(w) == 1,
          "clear does not rewind the epoch, which would readmit a stale writer");

    eshi_world_destroy(w);
}

/*
 * The headline property. Imperative construction under Dart's hot reload — which
 * re-runs build() without unwinding native state — produces a second set of
 * entities every time. A description reconciled by key cannot.
 */
void test_scene_reload_does_not_duplicate() {
    std::printf("scene reload does not duplicate\n");
    EshiWorld* w = make_world();

    uint32_t words[256];
    EshiEntity first_ball = ESHI_NULL_ENTITY;

    for (uint32_t reload = 1; reload <= 8; ++reload) {
        eshi::SceneWriter scene(words, 256);
        describe(scene, reload, 0.0f, true);
        check(scene.submit(w) == ESHI_OK, "every re-submission is accepted");

        if (reload == 1) first_ball = eshi_scene_entity(w, kKeyBall);
    }

    check(eshi_scene_node_count(w) == 3, "eight reloads leave three nodes");
    check(eshi_entity_count(w) == 3, "eight reloads leave three entities");
    /* Guarded, so the identity check below cannot pass by comparing two nulls. */
    check(first_ball != ESHI_NULL_ENTITY, "the first reload produced a real entity");
    check(eshi_scene_entity(w, kKeyBall) == first_ball,
          "a reloaded node keeps its identity rather than being recreated");

    /* Re-submitting the *same* epoch is a no-op too, not a stale rejection. */
    eshi::SceneWriter again(words, 256);
    describe(again, 8, 0.0f, true);
    check(again.submit(w) == ESHI_OK, "the current epoch may be re-submitted");
    check(eshi_entity_count(w) == 3, "re-submitting the same epoch changes nothing");

    eshi_world_destroy(w);
}

/*
 * The half of retained mode that makes hot reload usable rather than merely
 * safe: reconciling writes a component only when its *described* value changed,
 * so editing paddle speed in Dart does not also teleport the ball back to its
 * spawn point mid-rally.
 */
void test_scene_updates_only_what_changed() {
    std::printf("scene updates only what changed\n");
    EshiWorld* w = make_world();

    uint32_t words[256];
    eshi::SceneWriter initial(words, 256);
    describe(initial, 1, 0.0f, true);
    initial.submit(w);

    const EshiEntity ball = eshi_scene_entity(w, kKeyBall);

    /* Let the simulation carry the ball away from its described spawn. */
    for (int i = 0; i < 30; ++i) eshi_tick(w, 1.0f / 60.0f);

    float simulated_x = 0.0f;
    eshi_transform_get(w, ball, &simulated_x, NULL);
    check(simulated_x > 0.1f, "the ball moved under simulation");

    /* An identical description must leave the running simulation alone. */
    eshi::SceneWriter unchanged(words, 256);
    describe(unchanged, 2, 0.0f, true);
    unchanged.submit(w);

    float after_x = 0.0f;
    eshi_transform_get(w, ball, &after_x, NULL);
    check(after_x == simulated_x,
          "re-submitting an unchanged value does not stomp simulated state");

    /* An edited value must take effect — that is the point of the reload. */
    eshi::SceneWriter edited(words, 256);
    describe(edited, 3, 0.5f, true);
    edited.submit(w);

    float paddle_y = 0.0f;
    eshi_transform_get(w, eshi_scene_entity(w, kKeyPaddle), NULL, &paddle_y);
    check_near(paddle_y, 0.5f, 1e-6f, "an edited value is applied on reload");

    eshi_transform_get(w, ball, &after_x, NULL);
    check(after_x == simulated_x, "editing one node leaves the others simulating");

    eshi_world_destroy(w);
}

void test_scene_sweep_destroys_dropped_nodes() {
    std::printf("scene sweep\n");
    EshiWorld* w = make_world();

    uint32_t words[256];
    eshi::SceneWriter initial(words, 256);
    describe(initial, 1, 0.0f, true);
    initial.submit(w);

    const EshiEntity wall = eshi_scene_entity(w, kKeyWall);
    const EshiEntity ball = eshi_scene_entity(w, kKeyBall);
    check(eshi_entity_count(w) == 3, "three entities before the drop");

    /* Same scene minus the wall: the sweep must take exactly that one. */
    eshi::SceneWriter dropped(words, 256);
    describe(dropped, 2, 0.0f, false);
    dropped.submit(w);

    check(eshi_scene_node_count(w) == 2, "the dropped node is forgotten");
    check(eshi_entity_count(w) == 2, "and its entity is destroyed");
    check(eshi_entity_alive(w, wall) == 0, "the dropped entity is dead");
    check(eshi_entity_alive(w, ball) == 1, "the surviving entities are untouched");
    check(eshi_scene_entity(w, kKeyWall) == ESHI_NULL_ENTITY, "its key no longer resolves");
    check(eshi_scene_entity(w, kKeyBall) == ball, "surviving keys still resolve");

    eshi_world_destroy(w);
}

/*
 * A closure that outlived a hot reload and still holds a buffer will happily
 * flush the scene it was built for. Without the epoch gate that submission
 * sweeps away everything the new code just created.
 */
void test_scene_epoch_gate() {
    std::printf("scene epoch gate\n");
    EshiWorld* w = make_world();

    uint32_t words[256];
    eshi::SceneWriter current(words, 256);
    describe(current, 5, 0.0f, true);
    current.submit(w);
    check(eshi_entity_count(w) == 3, "the current scene is live");

    /* The stale writer describes a smaller scene at an older epoch. */
    uint32_t stale_words[256];
    eshi::SceneWriter stale(stale_words, 256);
    describe(stale, 4, 0.0f, false);

    check(stale.submit(w) == ESHI_ERR_STALE, "a stale scene is reported, not applied");
    check(eshi_scene_epoch(w) == 5, "a stale scene does not move the epoch back");
    check(eshi_entity_count(w) == 3, "a stale scene does not sweep the live one");
    check(eshi_scene_entity(w, kKeyWall) != ESHI_NULL_ENTITY,
          "the node the stale writer omitted survives");

    /* Input rides outside the scene vocabulary, so the gate must not eat it. */
    uint32_t input_words[8];
    eshi::SceneWriter keys(input_words, 8);
    keys.input(ESHI_KEY_SPACE, true);
    check(keys.submit(w) == ESHI_OK, "input is accepted");
    check(eshi_input_down(w, ESHI_KEY_SPACE) == 1, "input crosses in the command buffer");

    eshi_world_destroy(w);
}

/*
 * Scenes may span flushes, so the sweep runs on SCENE_END and nowhere else. The
 * consequence that matters: a submission cut short does not delete whatever did
 * not arrive.
 */
void test_scene_spans_flushes() {
    std::printf("scene spans flushes\n");
    EshiWorld* w = make_world();

    uint32_t words[256];
    eshi::SceneWriter initial(words, 256);
    describe(initial, 1, 0.0f, true);
    initial.submit(w);
    check(eshi_entity_count(w) == 3, "the initial scene is live");

    /* First half of a new epoch: opened, one node described, no end. */
    eshi::SceneWriter head(words, 256);
    head.begin(2).node(kKeyBall).transform(0.0f, 0.0f);
    check(head.submit(w) == ESHI_OK, "a partial scene is accepted");
    check(eshi_entity_count(w) == 3,
          "an unterminated scene sweeps nothing, so truncation cannot delete a scene");

    /* Second half, in its own flush. Now the sweep runs. */
    eshi::SceneWriter tail(words, 256);
    tail.node(kKeyPaddle).transform(-1.0f, 0.0f).end();
    check(tail.submit(w) == ESHI_OK, "the continuation is accepted");
    check(eshi_entity_count(w) == 2, "the sweep runs once the scene is closed");
    check(eshi_scene_entity(w, kKeyWall) == ESHI_NULL_ENTITY,
          "the node absent from the spanned scene is swept");

    eshi_world_destroy(w);
}

void test_event_buffer() {
    std::printf("event buffer\n");
    EshiWorld* w = make_world();

    /* Three mutually overlapping static colliders: three pairs, three events. */
    const uint32_t kLayer = 1u << 0;
    for (int i = 0; i < 3; ++i) {
        EshiEntity e = eshi_entity_create(w);
        eshi_transform_set(w, e, 0.05f * (float)i, 0.0f);
        eshi_collider_set(w, e, 0.5f, 0.5f, kLayer, kLayer, ESHI_COLLIDER_STATIC);
    }
    eshi_tick(w, 1.0f / 60.0f);

    check(eshi_events_reserve(w, 64) == ESHI_OK, "the event buffer can be reserved");

    uint32_t written = 0;
    check(eshi_events_pack(w, &written) == ESHI_OK, "events pack");
    check(written == 3 * 6, "three collision records, six words each");

    EshiCollisionEvent decoded[8];
    eshi::EventReader reader(eshi_events_data(w), written);
    int32_t count = 0;
    while (count < 8 && reader.next_collision(&decoded[count])) ++count;
    check(count == 3, "every packed record reads back");

    EshiCollisionEvent direct[8];
    const int32_t polled = eshi_collisions_poll(w, direct, 8);
    check(polled == 3, "the per-call path agrees on the count");
    check(decoded[0].a == direct[0].a && decoded[0].b == direct[0].b,
          "the packed record names the same pair as the per-call path");
    check(decoded[0].penetration == direct[0].penetration,
          "float payloads survive the word round-trip exactly");

    /*
     * Overflow is reported rather than hidden. eshi_collisions_poll() silently
     * truncates at `max`, which is how a game with a 16-event buffer misses the
     * seventeenth hit and never learns it happened.
     */
    EshiWorld* small = make_world();
    for (int i = 0; i < 3; ++i) {
        EshiEntity e = eshi_entity_create(small);
        eshi_transform_set(small, e, 0.05f * (float)i, 0.0f);
        eshi_collider_set(small, e, 0.5f, 0.5f, kLayer, kLayer, ESHI_COLLIDER_STATIC);
    }
    eshi_tick(small, 1.0f / 60.0f);
    eshi_events_reserve(small, 6);

    written = 0;
    check(eshi_events_pack(small, &written) == ESHI_ERR_LIMIT, "overflow is reported");
    check(written == 6, "and reports the whole records that did fit");

    eshi_world_destroy(small);
    eshi_world_destroy(w);
}

} /* namespace */

int main() {
    std::printf("larimar core tests\n\n");

    test_grade_gate();
    test_material_and_render();
    test_entity_generations();
    test_sparse_set_removal();
    test_motion_and_bounds();
    test_collision_layers();
    test_restitution_reflects();
    test_determinism();
    test_system_ordering();
    test_input_edges();

    test_command_wire_format();
    test_shared_command_buffer();
    test_scene_reconcile();
    test_scene_reload_does_not_duplicate();
    test_scene_updates_only_what_changed();
    test_scene_sweep_destroys_dropped_nodes();
    test_scene_epoch_gate();
    test_scene_spans_flushes();
    test_event_buffer();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
