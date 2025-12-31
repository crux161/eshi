#include "../glsl_core.h"
#include <math.h>

using namespace glsl;

SHADER_CTX vec4 palette(float t) {
    vec4 a = vec4(0.5f, 0.5f, 0.5f, 0.0f);
    vec4 b = vec4(0.5f, 0.5f, 0.5f, 0.0f);
    vec4 c = vec4(1.0f, 1.0f, 1.0f, 0.0f);
    vec4 d = vec4(0.263f, 0.416f, 0.557f, 0.0f);

    return a + b * cos((c * t + d) * 6.28318f);
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord * 2.0f - iResolution) / iResolution.y;
    vec2 uv0 = uv;
    
    vec4 finalColor = vec4(0.0f);
    
    for (float i = 0.0f; i < 4.0f; i++) {
        uv = fract(uv * 1.5f) - 0.5f;

        float d = length(uv) * exp(-length(uv0));

        vec4 col = palette(length(uv0) + i * 0.4f + iTime * 0.4f);

        d = sin(d * 8.0f + iTime) / 8.0f;
        d = abs(d); 
        
        d = pow(0.01f / d, 1.2f);

        finalColor += col * d;
    }
        
    finalColor.w = 1.0f;
    fragColor = finalColor;
}
