/**
 * @file pong.cpp
 * @brief First Light — Pong with no engine code in it.
 *
 * Compare against feature/pong-engine, where the game was a class inside
 * main.cpp, sat next to the event loop and the renderer selection, and forced
 * `GameData` into the shared math header and a fourth parameter onto every
 * backend's renderFrame().
 *
 * Here the game is: a uniform block it owns, a scene it builds, three systems,
 * and a shader. It contains no main(), no SDL call, no renderer reference, no
 * IGame, and no ResolvePaddleBounce — collision comes from generic colliders
 * and a generic solver, and the game only adds flavour by reading events.
 */
#include "pong.h"

#include <eshi/scene.hpp>

#include <cmath>
#include <cstdio>

namespace pong {

namespace {

/* Collision layers. */
const uint32_t kLayerBall   = 1u << 0;
const uint32_t kLayerPaddle = 1u << 1;
const uint32_t kLayerWall   = 1u << 2;

/*
 * Scene keys.
 *
 * The reconciler identifies an entity by its key, never by creation order, so
 * these are the stable half of the contract: the description may gain, lose or
 * reorder nodes and a paddle stays the same paddle. Dart will emit exactly
 * these numbers from its widget keys.
 */
const uint32_t kNodePaddleL    = 1;
const uint32_t kNodePaddleR    = 2;
const uint32_t kNodeBall       = 3;
const uint32_t kNodeWallTop    = 4;
const uint32_t kNodeWallBottom = 5;

/** Comfortably above the ~73 words the description below packs into. */
const uint32_t kSceneWords = 128;

const float kArenaX      = 1.77f;
const float kArenaY      = 1.0f;
const float kPaddleSpeed = 2.0f;
const float kPaddleHalfH = 0.2f;
const float kPaddleHalfW = 0.02f;
const float kBallRadius  = 0.03f;
const float kBallSpeed   = 1.1f;
const float kMaxBallSpeed = 3.0f;

/* ---------------------------------------------------------------------------
 * Systems — pure functions over the world. Registered, not called.
 * -------------------------------------------------------------------------*/

/** Reads abstract keys and drives paddle velocity. No SDL scancodes here. */
void system_paddles(EshiWorld* w, float /*dt*/, void* user) {
    Game* g = static_cast<Game*>(user);

    float left = 0.0f;
    if (eshi_input_down(w, ESHI_KEY_W)) left += 1.0f;
    if (eshi_input_down(w, ESHI_KEY_S)) left -= 1.0f;

    float right = 0.0f;
    if (eshi_input_down(w, ESHI_KEY_UP))   right += 1.0f;
    if (eshi_input_down(w, ESHI_KEY_DOWN)) right -= 1.0f;

    eshi_velocity_set(w, g->paddle_l, 0.0f, left * kPaddleSpeed);
    eshi_velocity_set(w, g->paddle_r, 0.0f, right * kPaddleSpeed);
}

/**
 * Game-specific collision response.
 *
 * The engine has already separated the bodies and reflected the velocity. All
 * that is left is the flavour: English off the paddle face, a speed-up, and
 * the hit flash. This is the split the old ResolvePaddleBounce() collapsed.
 */
void system_ball_response(EshiWorld* w, float dt, void* user) {
    Game* g = static_cast<Game*>(user);

    EshiCollisionEvent events[16];
    const int32_t n = eshi_collisions_poll(w, events, 16);

    for (int32_t i = 0; i < n; ++i) {
        const EshiCollisionEvent& ev = events[i];

        const bool involves_ball = (ev.a == g->ball || ev.b == g->ball);
        if (!involves_ball) continue;

        const EshiEntity other = (ev.a == g->ball) ? ev.b : ev.a;
        if (other != g->paddle_l && other != g->paddle_r) continue;

        float vx, vy;
        if (eshi_velocity_get(w, g->ball, &vx, &vy) != ESHI_OK) continue;

        float ball_y = 0.0f, paddle_y = 0.0f;
        eshi_transform_get(w, g->ball, NULL, &ball_y);
        eshi_transform_get(w, other, NULL, &paddle_y);

        /* Where the ball struck the paddle steers it. */
        vy += (ball_y - paddle_y) * 3.0f;
        vx *= 1.05f;

        const float speed = std::sqrt(vx * vx + vy * vy);
        if (speed > kMaxBallSpeed) {
            vx *= kMaxBallSpeed / speed;
            vy *= kMaxBallSpeed / speed;
        }

        eshi_velocity_set(w, g->ball, vx, vy);
        g->uniforms.hit_timer = 1.0f;
    }

    g->uniforms.hit_timer *= std::pow(0.05f, dt);

    /* Scoring: the ball leaving the arena is a point, not a collision. */
    float bx = 0.0f;
    if (eshi_transform_get(w, g->ball, &bx, NULL) == ESHI_OK) {
        if (bx > kArenaX + 0.2f)  { g->score_l++; serve(w, g, -1.0f); }
        if (bx < -kArenaX - 0.2f) { g->score_r++; serve(w, g,  1.0f); }
    }
}

/**
 * Packs component state into the uniform block the shader reads.
 *
 * This is the seam that replaced `GameData` in glsl_core.h. The engine moves
 * an opaque pointer; only these few lines and the shader know the layout. On
 * the Filament tier this same system feeds a material parameter buffer and the
 * game code does not change.
 */
void system_present(EshiWorld* w, float /*dt*/, void* user) {
    Game* g = static_cast<Game*>(user);

    eshi_transform_get(w, g->paddle_l, &g->uniforms.paddle_l.x, &g->uniforms.paddle_l.y);
    eshi_transform_get(w, g->paddle_r, &g->uniforms.paddle_r.x, &g->uniforms.paddle_r.y);
    eshi_transform_get(w, g->ball,     &g->uniforms.ball.x,     &g->uniforms.ball.y);

    g->uniforms.score_l = (float)g->score_l;
    g->uniforms.score_r = (float)g->score_r;
}

} /* namespace */

/* ---------------------------------------------------------------------------
 * Serve
 * -------------------------------------------------------------------------*/
void serve(EshiWorld* w, Game* g, float direction) {
    eshi_transform_set(w, g->ball, 0.0f, 0.0f);

    /* Drawn from the world RNG, so a seeded headless render stays reproducible. */
    const float angle = eshi_random_range(w, -0.4f, 0.4f);
    eshi_velocity_set(w,
                      g->ball,
                      direction * kBallSpeed * std::cos(angle),
                      kBallSpeed * std::sin(angle));
    g->uniforms.hit_timer = 0.0f;
}

namespace {

/* ---------------------------------------------------------------------------
 * Scene description
 *
 * Not a sequence of mutations — a description. Nothing here calls
 * eshi_entity_create(); the reconciler diffs this against the live world and
 * decides what to create, update or destroy. That is what makes it safe to run
 * again at any moment, which is the whole of §6.6.
 *
 * Note what is *absent*: the ball has no transform and no velocity. Those are
 * simulation state owned by serve(), not description, and declaring them here
 * would put the reconciler and the game in an argument over who owns the ball.
 * The rule that falls out — describe what the scene *is*, let systems own what
 * it is *doing* — is the one Dart will follow too.
 * -------------------------------------------------------------------------*/
void describe(eshi::SceneWriter& scene, uint32_t epoch) {
    scene.begin(epoch);

    /* Paddles: collide with the ball, never displaced by it. */
    scene.node(kNodePaddleL)
        .transform(-kArenaX + 0.1f, 0.0f)
        .velocity(0.0f, 0.0f)
        .collider(kPaddleHalfW, kPaddleHalfH,
                  kLayerPaddle, kLayerBall, ESHI_COLLIDER_STATIC)
        .bounds(-kArenaX + 0.1f, -kArenaX + 0.1f,
                -kArenaY + kPaddleHalfH, kArenaY - kPaddleHalfH);

    scene.node(kNodePaddleR)
        .transform(kArenaX - 0.1f, 0.0f)
        .velocity(0.0f, 0.0f)
        .collider(kPaddleHalfW, kPaddleHalfH,
                  kLayerPaddle, kLayerBall, ESHI_COLLIDER_STATIC)
        .bounds(kArenaX - 0.1f, kArenaX - 0.1f,
                -kArenaY + kPaddleHalfH, kArenaY - kPaddleHalfH);

    scene.node(kNodeBall)
        .collider(kBallRadius, kBallRadius,
                  kLayerBall, kLayerPaddle | kLayerWall, ESHI_COLLIDER_NONE)
        .restitution(1.0f);

    /* Top and bottom walls are ordinary static colliders, not an `if` in the loop. */
    scene.node(kNodeWallTop)
        .transform(0.0f, kArenaY + 0.1f)
        .collider(kArenaX + 1.0f, 0.1f, kLayerWall, kLayerBall, ESHI_COLLIDER_STATIC);

    scene.node(kNodeWallBottom)
        .transform(0.0f, -kArenaY - 0.1f)
        .collider(kArenaX + 1.0f, 0.1f, kLayerWall, kLayerBall, ESHI_COLLIDER_STATIC);

    scene.end();
}

/** Resolves the keys the game holds handles for. Cheap, and idempotent. */
void bind(EshiWorld* w, Game* g) {
    g->paddle_l = eshi_scene_entity(w, kNodePaddleL);
    g->paddle_r = eshi_scene_entity(w, kNodePaddleR);
    g->ball     = eshi_scene_entity(w, kNodeBall);
    g->wall[0]  = eshi_scene_entity(w, kNodeWallTop);
    g->wall[1]  = eshi_scene_entity(w, kNodeWallBottom);
}

EshiResult submit(EshiWorld* w, Game* g) {
    uint32_t words[kSceneWords];
    eshi::SceneWriter scene(words, kSceneWords);
    describe(scene, g->epoch);

    const EshiResult rc = scene.submit(w);
    if (rc == ESHI_OK) bind(w, g);
    return rc;
}

} /* namespace */

/* ---------------------------------------------------------------------------
 * Construction
 * -------------------------------------------------------------------------*/
void reload(EshiWorld* w, Game* g) {
    g->epoch++;

    /*
     * ESHI_ERR_STALE is not a failure here — it is the epoch gate doing its job
     * against a submission that lost a race, and the right response is to leave
     * the live scene alone. Anything else is a malformed description.
     */
    const EshiResult rc = submit(w, g);
    if (rc != ESHI_OK && rc != ESHI_ERR_STALE) {
        std::fprintf(stderr, "pong: scene reload failed: %s\n", eshi_result_string(rc));
    }
}

EshiResult build(EshiWorld* w, Game* g) {
    g->score_l = 0;
    g->score_r = 0;
    g->uniforms.hit_timer = 0.0f;
    g->epoch = 1;

    /*
     * A refused first submission leaves every handle null, and the symptom is a
     * game that renders and simulates nothing rather than one that crashes. Say
     * so here instead of letting it read as a physics bug.
     */
    const EshiResult rc = submit(w, g);
    if (rc != ESHI_OK) {
        std::fprintf(stderr, "pong: scene submission failed: %s\n", eshi_result_string(rc));
    }

    serve(w, g, 1.0f);

    eshi_system_add(w, "pong.paddles",  ESHI_ORDER_INPUT,       system_paddles,       g);
    eshi_system_add(w, "pong.response", ESHI_ORDER_COLLISION + 1, system_ball_response, g);
    eshi_system_add(w, "pong.present",  ESHI_ORDER_PRESENT,     system_present,       g);

    /*
     * One material, three representations. Ink executes the typed mainImage
     * below; lightweight GPU tiers transpile the sidecar; Filament loads the
     * matc package. Both GPU forms use the same flat uniform layout because a
     * shader toolchain cannot consume the game's C++ struct directly.
     */
    EshiMaterial material;
    material.cpu_shader = eshi::shader<Uniforms, mainImage>();
    material.source_path = "examples/pong/pong.gpu.cpp";
#ifdef ESHI_PONG_PACKAGE_PATH
    material.package_path = ESHI_PONG_PACKAGE_PATH;
#else
    material.package_path = NULL;
#endif
    material.uniform_data = &g->uniforms;
    material.uniform_size = sizeof(g->uniforms);

    /*
     * Returned rather than swallowed. A GPU tier that cannot find or compile
     * this material leaves the world with no backend, and every subsequent
     * frame is black — which previously reached the screen as a running game
     * with an empty arena, and reached a script as exit code zero.
     */
    return eshi_material_set(w, &material);
}

/* ---------------------------------------------------------------------------
 * Shader
 *
 * Unchanged in spirit from the original: SDF shapes, glow, a hit flash. The
 * difference is that its uniform block belongs to this file rather than to the
 * engine's shared math header. This is the fullscreen-material rendering model
 * that carries forward to Filament's unlit domain in Phase 1.
 * -------------------------------------------------------------------------*/
static float sd_box(sumi::vec2 p, sumi::vec2 b) {
    sumi::vec2 d = sumi::abs(p) - b;
    return sumi::length(sumi::max(d, sumi::vec2(0.0f))) +
           sumi::min(sumi::max(d.x, d.y), 0.0f);
}

static float glow(float d, float intensity) {
    return intensity / (d * d + 0.0001f);
}

void mainImage(sumi::vec4& fragColor, sumi::vec2 fragCoord,
               sumi::vec2 iResolution, float iTime, const Uniforms& u) {
    sumi::vec2 uv = (fragCoord * 2.0f - iResolution) / iResolution.y;
    sumi::vec3 col(0.0f);

    const float d_left  = sd_box(uv - u.paddle_l, sumi::vec2(kPaddleHalfW, kPaddleHalfH));
    const float d_right = sd_box(uv - u.paddle_r, sumi::vec2(kPaddleHalfW, kPaddleHalfH));
    const float d_ball  = sumi::length(uv - u.ball) - kBallRadius;

    col += sumi::vec3(0.2f, 0.8f, 1.0f) * glow(d_left, 0.002f);
    col += sumi::vec3(1.0f, 0.2f, 0.5f) * glow(d_right, 0.002f);
    col += sumi::vec3(1.0f, 1.0f, 1.0f) * glow(d_ball, 0.005f);

    /* Impact flash. */
    col += sumi::vec3(1.0f) * u.hit_timer * 0.35f *
           std::exp(-sumi::length(uv - u.ball) * 2.0f);

    /* Centre line, dashed by a fold on y. */
    if (std::fabs(uv.x) < 0.004f && sumi::fract(uv.y * 6.0f + iTime * 0.1f) < 0.5f) {
        col += sumi::vec3(0.18f);
    }

    /* Score pips along the top edge. */
    for (int i = 0; i < 9; ++i) {
        const float fx = 0.08f * (float)(i + 1);
        if (u.score_l > (float)i) {
            col += sumi::vec3(0.2f, 0.8f, 1.0f) *
                   glow(sumi::length(uv - sumi::vec2(-fx, kArenaY - 0.08f)) - 0.012f, 0.0004f);
        }
        if (u.score_r > (float)i) {
            col += sumi::vec3(1.0f, 0.2f, 0.5f) *
                   glow(sumi::length(uv - sumi::vec2(fx, kArenaY - 0.08f)) - 0.012f, 0.0004f);
        }
    }

    fragColor = sumi::vec4(col.x, col.y, col.z, 1.0f);
}

} /* namespace pong */
