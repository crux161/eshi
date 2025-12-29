#include "glsl_core.h"
#include <math.h>

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    // 1. Center UVs
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    // 2. Convert to Polar Coordinates
    // atan2 gives the angle around the center
    float angle = atan2f(uv.y, uv.x); 
    // length gives distance from center
    float dist = length(uv);

    // 3. Tunnel Math
    // Inverse distance creates the "depth" perspective
    float depth = 0.5f / dist; 

    // 4. Create the pattern
    // We warp the angle based on depth to create the spiral
    float pattern = sinf(depth * 10.0f - iTime * 4.0f) + 
                    sinf(angle * 6.0f + depth * 2.0f);

    // 5. Coloring
    // Use the pattern to drive RGB channels with offsets
    float r = sinf(pattern + 0.0f) * 0.5f + 0.5f;
    float g = sinf(pattern + 2.0f) * 0.5f + 0.5f;
    float b = sinf(pattern + 4.0f) * 0.5f + 0.5f;

    // Fade to black in the center (the far end of the tunnel)
    float fog = dist * 2.5f; 
    if (fog > 1.0f) fog = 1.0f;

    fragColor = vec4(r, g, b, 1.0f) * fog;
}
