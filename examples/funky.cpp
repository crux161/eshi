#include "../glsl_core.h"
using namespace glsl;

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.y;
    
    // Funky Plasma Effect
    float v = 0.0f;
    vec2 c = uv * 4.0f - vec2(2.0f);
    v += sin(c.x + iTime);
    v += sin((c.y + iTime) / 2.0f);
    v += sin((c.x + c.y + iTime) / 2.0f);
    c += vec2(sin(iTime / 3.0f), cos(iTime / 2.0f));
    v += sin(sqrt(c.x * c.x + c.y * c.y + 1.0f) + iTime);
    v = v / 2.0f;
    
    vec3 col = vec3(sin(3.14f * v), cos(3.14f * v), -sin(3.14f * v));
    fragColor = vec4(col.x * 0.5f + 0.5f, col.y * 0.5f + 0.5f, col.z * 0.5f + 0.5f, 1.0f);
}
