#include "../glsl_core.h"

using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    float distSq = dot(uv, uv);

    vec4 phases(
        uv.x * 10.0f + iTime * 2.0f,    
        uv.y * 10.0f - iTime * 3.0f,      
        distSq * 20.0f - iTime * 5.0f, 
        iTime                             
    );

    vec4 signal = sin(phases); 

    fragColor = signal * 0.5f + 0.5f;
    fragColor.w = 1.0f;
}


