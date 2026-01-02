#include "../glsl_core.h"
using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.y;
    
    
    float v = 0.0f;
    vec2 c = uv * 4.0f - vec2(2.0f);
    v += glsl::sin(c.x + iTime);
    v += glsl::sin((c.y + iTime) / 2.0f);
    v += glsl::sin((c.x + c.y + iTime) / 2.0f);
    c += vec2(glsl::sin(iTime / 3.0f), glsl::cos(iTime / 2.0f));
    v += glsl::sin(glsl::sqrt(c.x * c.x + c.y * c.y + 1.0f) + iTime);
    v = v / 2.0f;
    
    vec3 col = vec3(glsl::sin(3.14f * v), glsl::cos(3.14f * v), -glsl::sin(3.14f * v));
    fragColor = vec4(col.x * 0.5f + 0.5f, col.y * 0.5f + 0.5f, col.z * 0.5f + 0.5f, 1.0f);
}
