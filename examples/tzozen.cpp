#include "../glsl_core.h"
using namespace glsl;


SHADER_CTX inline vec4 tanh(vec4 v) {
    return vec4(::tanhf(v.x), ::tanhf(v.y), ::tanhf(v.z), ::tanhf(v.w));
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 r = iResolution;
    float t = iTime * 2.0f * 3.14159f; 

    vec2 p = (fragCoord * 2.0f - r) / r.y;
    vec2 l = vec2(0.0f);
    vec2 i = vec2(0.0f);
    
    
    l = l + (4.0f - 4.0f * glsl::abs(0.7f - dot(p, p)));
    vec2 v = p * l;
    
    vec4 o = vec4(0.0f); 
    
    for(; i.y++ < 8.0f; ) {
        
        o += (glsl::sin(vec4(v.x, v.y, v.y, v.x)) + 1.0f) * glsl::abs(v.x - v.y);
        v += glsl::cos(vec2(v.y, v.x) * i.y + i + t) / i.y + 0.7f;
    }
    
    
    o = tanh(5.0f * glsl::exp(l.x - 4.0f - p.y * vec4(-1, 1, 2, 0)) / o);

    fragColor = o;
}
