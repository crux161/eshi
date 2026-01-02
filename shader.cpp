#include "glsl_core.h"
using namespace glsl;


SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    
    vec2 uv = fragCoord / iResolution.y;
    vec3 col = 0.5f + 0.5f * cos(vec3(iTime, iTime + 2.0f, iTime + 4.0f) + uv.x);
    fragColor = vec4(col.x, col.y, col.z, 1.0f);
}
