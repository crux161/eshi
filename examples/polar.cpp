#include "../glsl_core.h"
#include <math.h>

using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    float angle = atan2(uv.y, uv.x); 
    float dist = length(uv);

    float depth = 0.5f / dist; 

    float pattern = sin(depth * 10.0f - iTime * 4.0f) + 
                    sin(angle * 6.0f + depth * 2.0f);

    float r = sin(pattern + 0.0f) * 0.5f + 0.5f;
    float g = sin(pattern + 2.0f) * 0.5f + 0.5f;
    float b = sin(pattern + 4.0f) * 0.5f + 0.5f;

    float fog = dist * 2.5f; 
    if (fog > 1.0f) fog = 1.0f;

    fragColor = vec4(r, g, b, 1.0f) * fog;
}
