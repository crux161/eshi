#include "glsl_core.h"
#include <math.h> 

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    
    
    vec2 b1 = vec2(cosf(iTime * 1.5f), sinf(iTime)) * 0.4f;
    
    vec2 b2 = vec2(sinf(iTime * 2.0f), cosf(iTime * 1.8f)) * 0.5f;
    
    vec2 b3 = vec2(cosf(iTime * 3.1f), sinf(iTime * 2.7f)) * 0.6f;

    
    float d1 = length(uv - b1);
    float d2 = length(uv - b2);
    float d3 = length(uv - b3);

    
    float field = (0.1f / d1) + (0.1f / d2) + (0.1f / d3);

    
    
    
    
    vec4 col = vec4(0.0f, 0.05f, 0.2f, 1.0f);

    if (field > 1.0f) {
        
        float rim = 0.0f; 
        if(field < 1.2f) rim = 1.0f; 
        
        vec4 coreColor = vec4(0.2f, 0.8f, 1.0f, 1.0f); 
        col = mix(col, coreColor + rim, 0.8f);
    }

    fragColor = col;
}
