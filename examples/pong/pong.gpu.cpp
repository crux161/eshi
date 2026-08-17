/**
 * @file pong.gpu.cpp
 * @brief Pong's material for the GPU tiers (Paper, Brush, Gold).
 *
 * This file is never compiled by the C++ compiler. It is read at runtime and
 * transpiled to GLSL or MSL by core/src/render/transpile.cpp, which is why it
 * carries no #include and no using-directive — the transpiler strips both, and
 * the resulting text would not be valid C++ anyway.
 *
 * It exists because pong.cpp's mainImage takes a typed `const Uniforms&`, and a
 * textual transpiler cannot lower a struct parameter into a shader binding. The
 * GPU tiers instead receive the block as a flat float array named
 * `eshi_uniforms`, so this file is the same material re-expressed against that
 * array. Same pattern as examples/gpu/, for the same reason.
 *
 * ---------------------------------------------------------------------------
 * UNIFORM LAYOUT CONTRACT — must match pong::Uniforms in pong.h.
 *
 *   [0] paddle_l.x   [1] paddle_l.y
 *   [2] paddle_r.x   [3] paddle_r.y
 *   [4] ball.x       [5] ball.y
 *   [6] hit_timer
 *   [7] score_l
 *   [8] score_r
 *
 * pong.h static_asserts that the struct is exactly these nine tightly packed
 * floats, so a layout drift is a build failure rather than a garbled frame.
 * ---------------------------------------------------------------------------
 *
 * One constraint worth knowing: every read of `eshi_uniforms` has to happen
 * inside mainImage. GLSL exposes it as a global, but MSL binds it as a kernel
 * argument threaded through mainImage's signature, so a helper function has no
 * way to see it. Hence the block of locals at the top.
 */

const float kPaddleHalfW = 0.02f;
const float kPaddleHalfH = 0.2f;
const float kBallRadius = 0.03f;
const float kArenaY = 1.0f;

SHADER_CTX float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, vec2(0.0f, 0.0f))) + min(max(d.x, d.y), 0.0f);
}

SHADER_CTX float glow(float d, float intensity) {
    return intensity / (d * d + 0.0001f);
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 paddleL = vec2(eshi_uniforms[0], eshi_uniforms[1]);
    vec2 paddleR = vec2(eshi_uniforms[2], eshi_uniforms[3]);
    vec2 ball = vec2(eshi_uniforms[4], eshi_uniforms[5]);
    float hitTimer = eshi_uniforms[6];
    float scoreL = eshi_uniforms[7];
    float scoreR = eshi_uniforms[8];

    vec2 uv = (fragCoord * 2.0f - iResolution) / iResolution.y;
    vec3 col = vec3(0.0f, 0.0f, 0.0f);

    float dLeft = sdBox(uv - paddleL, vec2(kPaddleHalfW, kPaddleHalfH));
    float dRight = sdBox(uv - paddleR, vec2(kPaddleHalfW, kPaddleHalfH));
    float dBall = length(uv - ball) - kBallRadius;

    col += vec3(0.2f, 0.8f, 1.0f) * glow(dLeft, 0.002f);
    col += vec3(1.0f, 0.2f, 0.5f) * glow(dRight, 0.002f);
    col += vec3(1.0f, 1.0f, 1.0f) * glow(dBall, 0.005f);

    col += vec3(1.0f, 1.0f, 1.0f) * hitTimer * 0.35f * exp(-length(uv - ball) * 2.0f);

    if (abs(uv.x) < 0.004f && fract(uv.y * 6.0f + iTime * 0.1f) < 0.5f) {
        col += vec3(0.18f, 0.18f, 0.18f);
    }

    for (int i = 0; i < 9; ++i) {
        float fx = 0.08f * float(i + 1);
        if (scoreL > float(i)) {
            col += vec3(0.2f, 0.8f, 1.0f) *
                   glow(length(uv - vec2(-fx, kArenaY - 0.08f)) - 0.012f, 0.0004f);
        }
        if (scoreR > float(i)) {
            col += vec3(1.0f, 0.2f, 0.5f) *
                   glow(length(uv - vec2(fx, kArenaY - 0.08f)) - 0.012f, 0.0004f);
        }
    }

    fragColor = vec4(col.x, col.y, col.z, 1.0f);
}
