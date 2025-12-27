#include "glsl_core.h"
#include <math.h> // Needed for sqrt, pow

void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    // 1. Center coordinates (-1 to 1)
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    // 2. Create moving points (Metaballs)
    // Ball 1: Moves in a large circle
    vec2 b1 = vec2(cosf(iTime * 1.5f), sinf(iTime)) * 0.4f;
    // Ball 2: Moves in a figure-eight
    vec2 b2 = vec2(sinf(iTime * 2.0f), cosf(iTime * 1.8f)) * 0.5f;
    // Ball 3: Moves wildly
    vec2 b3 = vec2(cosf(iTime * 3.1f), sinf(iTime * 2.7f)) * 0.6f;

    // 3. Calculate distance from pixel to balls
    float d1 = length(uv - b1);
    float d2 = length(uv - b2);
    float d3 = length(uv - b3);

    // 4. Metaball formula: Sum of (Radius / Distance)
    float field = (0.1f / d1) + (0.1f / d2) + (0.1f / d3);

    // 5. Thresholding (The "Goo" effect)
    // If the field is strong, make it bright.
    
    // Background color (Deep Blue)
    vec4 col = vec4(0.0f, 0.05f, 0.2f, 1.0f);

    if (field > 1.0f) {
        // Blob color (interpolates based on field strength)
        float rim = 0.0f; 
        if(field < 1.2f) rim = 1.0f; // Simple rim lighting
        
        vec4 coreColor = vec4(0.2f, 0.8f, 1.0f, 1.0f); // Cyan
        col = mix(col, coreColor + rim, 0.8f);
    }

    fragColor = col;
}
