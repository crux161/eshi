#include "../glsl_core.h"


using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    
    float angle = glsl::atan(uv.y, uv.x); 
    float dist = glsl::length(uv);

    float depth = 0.5f / dist; 

    
    float pattern = glsl::sin(depth * 10.0f - iTime * 4.0f) + 
                    glsl::sin(angle * 6.0f + depth * 2.0f);

    float r = glsl::sin(pattern + 0.0f) * 0.5f + 0.5f;
    float g = glsl::sin(pattern + 2.0f) * 0.5f + 0.5f;
    float b = glsl::sin(pattern + 4.0f) * 0.5f + 0.5f;

    float fog = dist * 2.5f; 
    if (fog > 1.0f) fog = 1.0f;

    fragColor = vec4(r, g, b, 1.0f) * fog;
}
