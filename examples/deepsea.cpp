#include "../glsl_core.h"
using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;
    
    float t = iTime * 0.5f;
    vec3 col = vec3(0.0f);
    
    for(float i=0.0f; i<3.0f; i+=1.0f) {
        vec2 p = uv;
        p.x += glsl::sin(t + p.y * (2.0f + i)) * 0.2f;
        p.y += glsl::cos(t + p.x * (1.5f + i)) * 0.2f;
        
        float d = glsl::length(p);
        
        
        
        col[int(i)] = 0.05f / (glsl::abs(d - 0.5f) + 0.001f);
    }
    
    fragColor = vec4(col.x, col.y, col.z, 1.0f);
}
