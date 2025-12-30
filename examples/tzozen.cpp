#include "../glsl_core.h"
#include <math.h>

using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    
    vec2 r = iResolution;
    float t = iTime * 2.0f * M_PI; 

    vec2 p = (fragCoord * 2.0f - r) / r.y;
    vec2 l, i, v = p * (l += 4.0f - 4.0f * abs(0.7f - dot(p, p)));
    
    vec4 o;
    for(; i.y++ < 8.0f; o += (sin(v.xyyx()) + 1.0f) * abs(v.x - v.y)) {
        v += cos(v.yx() * i.y + i + t) / i.y + 0.7f;
    }
    
    o = tanh(5.0f * exp(l.x - 4.0f - p.y * vec4(-1, 1, 2, 0)) / o);

    fragColor = o;
}
