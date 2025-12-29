#include "glsl_core.h"
#include <math.h>


inline float hash(vec2 p) {
    p = vec2(dot(p, vec2(127.1f, 311.7f)), dot(p, vec2(269.5f, 183.3f)));
    return fract(sinf(p.x) * 43758.5453f);
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.y;
    
    // Background Color
    vec4 col = vec4(0.0f, 0.05f, 0.1f, 1.0f);
    col = mix(col, vec4(0.0f, 0.02f, 0.15f, 1.0f), length(uv - 0.5f));

    // Grid Logic
    vec2 gridUV = uv * 6.0f;
    vec2 gridID = floor(gridUV);
    
    // This line previously failed because fract(vec2) didn't exist
    vec2 gridST = fract(gridUV) - 0.5f; 

    float rnd = hash(gridID);
    float t = iTime + rnd * 10.0f;
    float life = fract(t * 0.5f); 
    
    float maxRadius = 0.45f;
    float currentRadius = life * maxRadius;
    float dist = length(gridST);
    float thickness = 0.02f;
    
    if (dist < currentRadius && dist > currentRadius - thickness) {
        float alpha = (1.0f - life);
        float ring = smoothstep(thickness, 0.0f, abs(dist - (currentRadius - thickness*0.5f)));
        col += vec4(0.4f, 0.8f, 1.0f, 1.0f) * ring * alpha * 2.0f;
    }
    
    if (hash(gridID + 1.0f) > 0.8f) {
        float sparkle = sinf(iTime * 10.0f + rnd * 100.0f);
        if (sparkle > 0.95f) col += 0.1f;
    }

    fragColor = col;
}
