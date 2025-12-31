#include "../glsl_core.h"
using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime, GameData /*game*/) {
    vec2 r = iResolution;
    float t = iTime * 2.0f * 3.14159f; 

    vec2 p = (fragCoord * 2.0f - r) / r.y;
    vec2 l = vec2(0.0f);
    vec2 i = vec2(0.0f);
    
    // Logic: l += scalar. Since we defined operator+(float, vec2) and operator+=(vec2),
    // we must write: l = l + scalar
    l = l + (4.0f - 4.0f * abs(0.7f - dot(p, p)));
    vec2 v = p * l;
    
    vec4 o = vec4(0.0f); 
    
    for(; i.y++ < 8.0f; ) {
        // v.xyyx() -> vec4(v.x, v.y, v.y, v.x)
        o += (sin(vec4(v.x, v.y, v.y, v.x)) + 1.0f) * abs(v.x - v.y);
        
        // v.yx() -> vec2(v.y, v.x)
        v += cos(vec2(v.y, v.x) * i.y + i + t) / i.y + 0.7f;
    }
    
    // p.y * vec4 -> scalar * vec4 (Commutative op added to glsl_core)
    o = tanh(5.0f * exp(l.x - 4.0f - p.y * vec4(-1, 1, 2, 0)) / o);

    fragColor = o;
}
