#include "../glsl_core.h"
using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime, GameData /*game*/) {
    vec2 uv = fragCoord / iResolution.y * 0.8f;
    float t = iTime * 0.5f;
    
    float v1 = sin(uv.x * 10.0f + t);
    float v2 = sin(uv.y * 10.0f + t * 0.5f);
    float v3 = sin((uv.x + uv.y) * 10.0f + t);
    
    float c = v1 + v2 + v3;
    
    // Warp the coordinates
    uv.x += sin(uv.y * 5.0f + c) * 0.5f;
    uv.y += cos(uv.x * 5.0f + c) * 0.5f;
    
    // Generate color patterns
    float r = sin(uv.x * 5.0f + t) * 0.5f + 0.5f;
    float g = sin(uv.y * 5.0f + t * 1.3f) * 0.5f + 0.5f;
    float b = sin((uv.x + uv.y) * 5.0f + t * 1.7f) * 0.5f + 0.5f;
    
    fragColor = vec4(r, g, b, 1.0f);
}
