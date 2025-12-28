#include "glsl_core.h"
#include <math.h>

void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.y;

    float r = 0.5f + 0.5f * sinf(iTime + uv.x);
    float g = 0.5f + 0.5f * sinf(iTime + uv.y + 2.0f);
    float b = 0.5f + 0.5f * sinf(iTime + uv.x + 4.0f);

    fragColor = vec4(r, g, b, 1.0f);
}
