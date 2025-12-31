#include "../glsl_core.h"

// Using namespace avoids the need for glsl:: prefix and simplifies GPU nesting
using namespace glsl;

// --- PART 1: GAME LOGIC (CPU) ---
// Note: We removed the wrapping namespace glsl {} block to let 'using namespace' handle it.
// This prevents the gpu::glsl:: namespace confusion.

SHADER_CTX void updateGame(GameData& s, const InputState& input) {
    const float PADDLE_SPEED = 2.0f * input.dt; // Fixed 'in' -> 'input'
    const float PADDLE_H = 0.2f;
    const float BOUND_Y = 1.0f;

    // 1. Paddle Movement
    if (input.w) s.paddleL.y += PADDLE_SPEED;
    if (input.s) s.paddleL.y -= PADDLE_SPEED;
    if (input.up)    s.paddleR.y += PADDLE_SPEED;
    if (input.down) s.paddleR.y -= PADDLE_SPEED;

    // Clamp Paddles
    s.paddleL.y = clamp(s.paddleL.y, -BOUND_Y + PADDLE_H, BOUND_Y - PADDLE_H);
    s.paddleR.y = clamp(s.paddleR.y, -BOUND_Y + PADDLE_H, BOUND_Y - PADDLE_H);

    // 2. Ball Physics
    s.ballPos += s.ballVel;

    // Bounce off Top/Bottom
    if (s.ballPos.y > BOUND_Y || s.ballPos.y < -BOUND_Y) s.ballVel.y *= -1.0f;

    // Bounce off Paddles (Simple AABB)
    // Left Paddle
    if (s.ballPos.x < s.paddleL.x + 0.05f && s.ballPos.x > s.paddleL.x - 0.05f &&
        s.ballPos.y < s.paddleL.y + PADDLE_H && s.ballPos.y > s.paddleL.y - PADDLE_H) {
        s.ballVel.x *= -1.1f;
        s.ballPos.x = s.paddleL.x + 0.06f;
        s.hitTimer = 1.0f; 
    }
    // Right Paddle
    if (s.ballPos.x > s.paddleR.x - 0.05f && s.ballPos.x < s.paddleR.x + 0.05f &&
        s.ballPos.y < s.paddleR.y + PADDLE_H && s.ballPos.y > s.paddleR.y - PADDLE_H) {
        s.ballVel.x *= -1.1f;
        s.ballPos.x = s.paddleR.x - 0.06f;
        s.hitTimer = 1.0f;
    }

    // Reset if out of bounds
    if (abs(s.ballPos.x) > 2.0f) {
        s.ballPos = vec2(0.0f);
        s.ballVel = vec2(0.03f, 0.01f);
    }

    s.hitTimer *= 0.95f;
}

// --- PART 2: GRAPHICS (GPU) ---

SHADER_CTX float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f);
}

SHADER_CTX float glow(float d, float intensity) {
    return intensity / (d * d + 0.0001f);
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime, GameData game) {
    vec2 uv = (fragCoord * 2.0f - iResolution) / iResolution.y;
    vec3 col = vec3(0.0f);

    // Shapes
    float dP1 = sdBox(uv - game.paddleL, vec2(0.02f, 0.2f));
    float dP2 = sdBox(uv - game.paddleR, vec2(0.02f, 0.2f));
    float dBall = length(uv - game.ballPos) - 0.03f;

    // Coloring
    col += vec3(0.2f, 0.8f, 1.0f) * glow(dP1, 0.002f); 
    col += vec3(1.0f, 0.2f, 0.5f) * glow(dP2, 0.002f); 
    col += vec3(1.0f, 1.0f, 1.0f) * glow(dBall, 0.005f); 

    // Impact Flash
    col += vec3(1.0f) * game.hitTimer * 0.2f * exp(-length(uv - game.ballPos));

    // Simple Grid
    if (abs(uv.x) < 0.005f) col += 0.2f; 

    fragColor = vec4(col.x, col.y, col.z, 1.0f);
}
