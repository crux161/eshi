// Flutter asset form of examples/pong/pong.gpu.cpp.
// Uniform layout: left paddle xy, right paddle xy, ball xy, hit, score l/r.
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
    col += vec3(1.0f, 1.0f, 1.0f) * hitTimer * 0.35f *
           exp(-length(uv - ball) * 2.0f);

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
