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

#include <cmath>
#include <cstdio>

namespace pong {

namespace {

/* Collision layers. */
const uint32_t kLayerBall   = 1u << 0;
const uint32_t kLayerPaddle = 1u << 1;
const uint32_t kLayerWall   = 1u << 2;

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

/* ---------------------------------------------------------------------------
 * Scene construction
 *
 * Flat, declarative, and side-effect free apart from the world it is handed.
 * That shape is what Phase 3's Dart reconciler will diff against — it is
 * already a description of a scene rather than a sequence of mutations
 * entangled with a frame loop.
 * -------------------------------------------------------------------------*/
void build(EshiWorld* w, Game* g) {
    g->score_l = 0;
    g->score_r = 0;
    g->uniforms.hit_timer = 0.0f;

    /* Paddles: collide with the ball, never displaced by it. */
    g->paddle_l = eshi_entity_create(w);
    eshi_transform_set(w, g->paddle_l, -kArenaX + 0.1f, 0.0f);
    eshi_velocity_set(w, g->paddle_l, 0.0f, 0.0f);
    eshi_collider_set(w, g->paddle_l, kPaddleHalfW, kPaddleHalfH,
                      kLayerPaddle, kLayerBall, ESHI_COLLIDER_STATIC);
    eshi_bounds_set(w, g->paddle_l,
                    -kArenaX + 0.1f, -kArenaX + 0.1f,
                    -kArenaY + kPaddleHalfH, kArenaY - kPaddleHalfH);

    g->paddle_r = eshi_entity_create(w);
    eshi_transform_set(w, g->paddle_r, kArenaX - 0.1f, 0.0f);
    eshi_velocity_set(w, g->paddle_r, 0.0f, 0.0f);
    eshi_collider_set(w, g->paddle_r, kPaddleHalfW, kPaddleHalfH,
                      kLayerPaddle, kLayerBall, ESHI_COLLIDER_STATIC);
    eshi_bounds_set(w, g->paddle_r,
                    kArenaX - 0.1f, kArenaX - 0.1f,
                    -kArenaY + kPaddleHalfH, kArenaY - kPaddleHalfH);

    /* Ball. */
    g->ball = eshi_entity_create(w);
    eshi_collider_set(w, g->ball, kBallRadius, kBallRadius,
                      kLayerBall, kLayerPaddle | kLayerWall, ESHI_COLLIDER_NONE);
    eshi_collider_set_restitution(w, g->ball, 1.0f);

    /* Top and bottom walls are ordinary static colliders, not an `if` in the loop. */
    const float wall_y[2] = { kArenaY + 0.1f, -kArenaY - 0.1f };
    for (int i = 0; i < 2; ++i) {
        g->wall[i] = eshi_entity_create(w);
        eshi_transform_set(w, g->wall[i], 0.0f, wall_y[i]);
        eshi_collider_set(w, g->wall[i], kArenaX + 1.0f, 0.1f,
                          kLayerWall, kLayerBall, ESHI_COLLIDER_STATIC);
    }

    serve(w, g, 1.0f);

    eshi_system_add(w, "pong.paddles",  ESHI_ORDER_INPUT,       system_paddles,       g);
    eshi_system_add(w, "pong.response", ESHI_ORDER_COLLISION + 1, system_ball_response, g);
    eshi_system_add(w, "pong.present",  ESHI_ORDER_PRESENT,     system_present,       g);

    /*
     * Ink only, for now: the GPU tiers need a transpilable source file, and
     * this shader takes a typed struct that the textual transpiler cannot
     * lower. A GPU sidecar written against the flat `eshi_uniforms` array
     * would lift that — the same pattern examples/gpu/ already uses.
     */
    EshiMaterial material;
    material.cpu_shader = eshi::shader<Uniforms, mainImage>();
    material.source_path = NULL;
    material.uniform_data = &g->uniforms;
    material.uniform_size = sizeof(g->uniforms);
    eshi_material_set(w, &material);
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
