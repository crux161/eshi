#include "../glsl_core.h"
using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;
    
    vec3 col = vec3(0.0f);
    vec2 z = uv;
    float t = iTime * 0.2f;
    
    
    vec2 c = vec2(glsl::sin(t), glsl::cos(t)) * 0.7f;
    
    float iter = 0.0f;
    for(float i=0.0f; i<32.0f; i+=1.0f) {
        
        float x = (z.x * z.x - z.y * z.y) + c.x;
        float y = (2.0f * z.x * z.y) + c.y;
        z = vec2(x, y);
        
        
        if(glsl::length(z) > 4.0f) break;
        iter += 1.0f;
    }
    
    float val = iter / 32.0f;
    
    col = vec3(val, val * 0.5f, glsl::sin(val * 6.0f));
    
    fragColor = vec4(col.x, col.y, col.z, 1.0f);
}
