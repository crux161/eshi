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

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
