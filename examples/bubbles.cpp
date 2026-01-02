#include "../glsl_core.h"
using namespace glsl;

SHADER_CTX inline float hash(vec2 p) {
    return glsl::fract(glsl::sin(glsl::dot(p, vec2(12.9898f, 78.233f))) * 43758.5453f);
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.y;
    vec2 p = uv * 8.0f;
    vec2 i = glsl::floor(p);
    vec2 f = glsl::fract(p);
    
    float t = iTime * 0.5f;
    float v = 0.0f;
    
    for(int y=-1; y<=1; y++) {
        for(int x=-1; x<=1; x++) {
            vec2 g = vec2((float)x, (float)y);
            vec2 r = g - f + hash(i + g);
            float d = glsl::length(r);
            float s = 0.5f + 0.5f * glsl::sin(t + hash(i + g) * 6.2831f);
            float size = 0.3f * s;
            
            
            
            
            v += 1.0f - glsl::smoothstep(size - 0.05f, size, d);
        }
    }
    
    vec3 col = vec3(0.2f, 0.5f, 1.0f) * v;
    fragColor = vec4(col.x, col.y, col.z, 1.0f);
}
