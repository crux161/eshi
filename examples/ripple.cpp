#include "../glsl_core.h"
using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.y;
    vec2 p = uv * 2.0f - 1.0f;
    
    float len = glsl::length(p);
    
    
    vec2 offset = vec2(0.0f);
    if (len > 0.001f) {
        offset = p/len * glsl::cos(vec2(0.0f, -iTime * 4.0f) + len * 20.0f - iTime * 2.0f) * 0.05f;
    }
    
    vec2 rip = p + offset;
    
    float val = glsl::length(rip);
    
    
    vec3 col = vec3(val, val * 0.5f, glsl::sin(val * 6.0f + iTime));
    
    fragColor = vec4(col.x, col.y, col.z, 1.0f);
}
