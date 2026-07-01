#include "../glsl_core.h"

using namespace glsl;

SHADER_CTX vec2 rotate2(vec2 p, float a) {
    float s = glsl::sin(a);
    float c = glsl::cos(a);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

SHADER_CTX float hash21(vec2 p) {
    return glsl::fract(glsl::sin(glsl::dot(p, vec2(127.1f, 311.7f))) * 43758.5453123f);
}

SHADER_CTX float noise2(vec2 p) {
    vec2 i = glsl::floor(p);
    vec2 f = glsl::fract(p);
    vec2 u = f * f * (3.0f - 2.0f * f);

    float a = hash21(i + vec2(0.0f, 0.0f));
    float b = hash21(i + vec2(1.0f, 0.0f));
    float c = hash21(i + vec2(0.0f, 1.0f));
    float d = hash21(i + vec2(1.0f, 1.0f));

    return glsl::mix(glsl::mix(a, b, u.x), glsl::mix(c, d, u.x), u.y);
}

SHADER_CTX float fbm2(vec2 p, int octaves) {
    float value = 0.0f;
    float amplitude = 0.5f;

    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise2(p);
        p = rotate2(p * 2.02f, 0.47f);
        amplitude *= 0.52f;
    }

    return value;
}

SHADER_CTX vec3 auroraPalette(float t) {
    vec3 deepGreen = vec3(0.05f, 0.85f, 0.48f);
    vec3 iceBlue = vec3(0.18f, 0.62f, 1.0f);
    vec3 violet = vec3(0.82f, 0.2f, 1.0f);

    vec3 col = glsl::mix(deepGreen, iceBlue, glsl::smoothstep(0.15f, 0.65f, t));
    return glsl::mix(col, violet, glsl::smoothstep(0.65f, 1.0f, t));
}

SHADER_CTX float starLayer(vec2 uv, float scale, float seed, float iTime) {
    vec2 p = rotate2(uv, 0.08f) * scale;
    vec2 cell = glsl::floor(p);
    vec2 local = glsl::fract(p);

    float h = hash21(cell + vec2(seed));
    vec2 star = vec2(hash21(cell + vec2(seed, 13.7f)),
                     hash21(cell + vec2(7.1f, seed)));

    float d = glsl::length(local - star);
    float body = glsl::smoothstep(0.04f, 0.0f, d);
    float twinkle = 0.65f + 0.35f * glsl::sin(iTime * (1.3f + h * 2.0f) + h * 37.0f);

    return body * glsl::step(0.965f, h) * twinkle;
}

SHADER_CTX float auroraBand(vec2 uv, float t, float offset, float width, float lean) {
    vec2 p = uv;
    p.x += offset + t * 0.16f;
    p = rotate2(p, lean);

    float drift = fbm2(vec2(p.x * 1.2f + t * 0.7f, offset), 5);
    float small = fbm2(vec2(p.x * 4.0f - t * 1.4f, p.y * 0.8f + offset), 4);
    float center = 0.26f + drift * 0.55f + 0.08f * glsl::sin(p.x * 2.6f + t * 2.1f);
    float curtain = glsl::smoothstep(width, 0.0f, glsl::abs(p.y - center));

    float lanes = glsl::fract((p.x + small * 0.9f) * 11.0f);
    lanes = glsl::pow(1.0f - glsl::abs(lanes - 0.5f) * 2.0f, 2.6f);

    float taper = glsl::smoothstep(-0.12f, 0.25f, p.y) * (1.0f - glsl::smoothstep(1.05f, 1.35f, p.y));
    float pulse = 0.78f + 0.22f * glsl::sin(t * 3.0f + offset * 11.0f);

    return curtain * taper * pulse * (0.45f + lanes * 0.9f);
}

SHADER_CTX float mountainHeight(float x, float t) {
    float n = fbm2(vec2(x * 1.7f + t * 0.015f, 4.2f), 5);
    float peaks = fbm2(vec2(x * 5.3f - t * 0.01f, 18.0f), 3);
    return -0.28f + 0.11f + n * 0.16f + peaks * 0.045f;
}

SHADER_CTX vec3 renderSky(vec2 uv, float iTime) {
    float t = iTime * 0.28f;
    float height = glsl::clamp(uv.y * 0.65f + 0.45f, 0.0f, 1.0f);

    vec3 low = vec3(0.025f, 0.055f, 0.11f);
    vec3 high = vec3(0.006f, 0.011f, 0.035f);
    vec3 col = glsl::mix(low, high, height);

    float stars = starLayer(uv + vec2(0.0f, 0.25f), 76.0f, 2.1f, iTime);
    stars += starLayer(uv + vec2(0.0f, 0.25f), 132.0f, 8.8f, iTime) * 0.45f;
    stars *= glsl::smoothstep(-0.05f, 0.42f, uv.y);
    col += vec3(1.0f, 0.92f, 0.78f) * stars;

    vec2 moonPos = uv - vec2(-0.58f, 0.62f);
    float moon = glsl::smoothstep(0.115f, 0.105f, glsl::length(moonPos));
    float cutout = glsl::smoothstep(0.105f, 0.095f, glsl::length(moonPos - vec2(0.04f, 0.025f)));
    float glow = glsl::smoothstep(0.42f, 0.0f, glsl::length(moonPos));
    col += vec3(0.76f, 0.84f, 1.0f) * glow * 0.18f;
    col += vec3(1.0f, 0.92f, 0.72f) * glsl::max(moon - cutout, 0.0f);

    float a0 = auroraBand(uv, t, 0.0f, 0.33f, -0.18f);
    float a1 = auroraBand(uv + vec2(0.22f, -0.05f), t * 1.18f, 3.7f, 0.22f, 0.11f);
    float a2 = auroraBand(uv + vec2(-0.35f, 0.03f), t * 0.86f, 7.3f, 0.18f, -0.05f);

    vec3 aurora = auroraPalette(glsl::fract(uv.x * 0.35f + t * 0.18f)) * a0;
    aurora += auroraPalette(glsl::fract(uv.x * 0.25f + 0.36f)) * a1 * 0.8f;
    aurora += vec3(0.55f, 0.9f, 1.0f) * a2 * 0.55f;

    col += aurora * (0.75f + height * 0.65f);
    col += vec3(0.03f, 0.13f, 0.12f) * glsl::smoothstep(-0.22f, 0.18f, uv.y);

    return col;
}

SHADER_CTX vec3 applyMountains(vec2 uv, vec3 col, float iTime) {
    float ridge = mountainHeight(uv.x, iTime);
    float horizon = -0.28f;
    float mask = glsl::smoothstep(ridge + 0.01f, ridge - 0.01f, uv.y);
    mask *= glsl::smoothstep(horizon - 0.02f, horizon + 0.06f, uv.y);

    vec3 mountain = vec3(0.008f, 0.012f, 0.025f);
    mountain += vec3(0.015f, 0.035f, 0.045f) * glsl::smoothstep(horizon, ridge + 0.04f, uv.y);

    return glsl::mix(col, mountain, mask);
}

SHADER_CTX vec3 renderWater(vec2 uv, float iTime) {
    float horizon = -0.28f;
    float t = iTime * 0.25f;
    float depth = glsl::clamp((horizon - uv.y) * 1.55f, 0.0f, 1.0f);

    float ripple = fbm2(vec2(uv.x * 18.0f + t * 1.8f, uv.y * 34.0f - t), 4);
    float fineRipple = glsl::sin((uv.y + ripple * 0.025f) * 260.0f - iTime * 4.0f);

    vec2 reflectedUv = vec2(uv.x + (ripple - 0.5f) * 0.09f * (0.2f + depth),
                            horizon + (horizon - uv.y) * 0.72f);

    vec3 reflected = applyMountains(reflectedUv, renderSky(reflectedUv, iTime), iTime);
    vec3 waterBase = vec3(0.006f, 0.022f, 0.04f);
    vec3 col = glsl::mix(reflected * vec3(0.34f, 0.54f, 0.62f), waterBase, depth);

    float moonPath = glsl::smoothstep(0.26f, 0.0f, glsl::abs(uv.x + 0.58f + ripple * 0.12f));
    moonPath *= glsl::smoothstep(0.95f, 0.05f, depth);
    col += vec3(0.45f, 0.62f, 0.85f) * moonPath * (0.18f + 0.12f * fineRipple);

    col *= 0.82f + 0.18f * fineRipple;
    col += vec3(0.02f, 0.11f, 0.09f) * glsl::smoothstep(0.05f, 0.0f, glsl::abs(uv.y - horizon));

    return col;
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;
    float horizon = -0.28f;

    vec3 sky = applyMountains(uv, renderSky(uv, iTime), iTime);
    vec3 water = renderWater(uv, iTime);
    float waterMask = 1.0f - glsl::smoothstep(horizon - 0.012f, horizon + 0.012f, uv.y);

    vec3 col = glsl::mix(sky, water, waterMask);
    float vignette = glsl::smoothstep(1.35f, 0.18f, glsl::length(uv * vec2(0.78f, 1.0f)));
    col *= 0.72f + 0.28f * vignette;

    col = glsl::clamp(col, 0.0f, 1.0f);
    col = glsl::pow(col, vec3(0.4545f));
    fragColor = vec4(col, 1.0f);
}
