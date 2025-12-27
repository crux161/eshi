#include "glsl_core.h"
#include <math.h>

void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    // 1. Zoom and Pan
    // Zoom oscillates over time
    float zoom = 1.5f + sinf(iTime * 0.5f) * 0.5f;
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;
    
    // Pan coordinates
    vec2 c = uv * zoom;
    c.x -= 0.5f; // Center the interesting part

    // 2. Julia Set / Mandelbrot Logic
    vec2 z = c;
    float iter = 0.0f;
    const float max_iter = 32.0f;

    // The "Julia" seed changes with time
    vec2 seed = vec2(sinf(iTime * 0.5f) * 0.7f, cosf(iTime * 0.3f) * 0.7f);

    for (float i = 0.0f; i < max_iter; i++) {
        // Complex Math: z = z^2 + seed
        // (x + iy)^2 = x^2 - y^2 + 2ixy
        float x = (z.x * z.x - z.y * z.y) + seed.x;
        float y = (2.0f * z.x * z.y) + seed.y;
        
        z = vec2(x, y);

        // If point escapes circle of radius 2
        if (dot(z, z) > 4.0f) break;
        iter++;
    }

    // 3. Coloring based on how long it took to escape
    float t = iter / max_iter;
    
    // Palette: Gold and Black
    float r = t * 2.0f; 
    float g = t * 1.0f;
    float b = t * 0.2f;

    fragColor = vec4(r, g, b, 1.0f);
}
