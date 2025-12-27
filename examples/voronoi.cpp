#include "glsl_core.h"
#include <math.h>

// --- Random Hash Function ---
// Returns a random vec2 between 0-1 based on an input vec2
inline vec2 hash2(vec2 p) {
    // Standard pseudo-random hash
    p = vec2(dot(p, vec2(127.1f, 311.7f)),
             dot(p, vec2(269.5f, 183.3f)));
    
    // fract(sin(p)*43758.5453)
    vec4 s = sin(p.xyyx()); // Using your library's sin
    // Standard fract implementation: x - floor(x)
    return vec2(s.x * 43758.5453f - floorf(s.x * 43758.5453f), 
                s.y * 43758.5453f - floorf(s.y * 43758.5453f));
}

void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.y;
    
    // Scale up to make a grid
    uv = uv * 5.0f;
    
    // Split into integer (grid ID) and fractional (local coord) parts
    vec2 i_st = vec2(floorf(uv.x), floorf(uv.y));
    vec2 f_st = uv - i_st;

    float m_dist = 1.0f;  // Minimum distance

    // Check neighbor cells (3x3 grid around current pixel)
    for (int y= -1; y <= 1; y++) {
        for (int x= -1; x <= 1; x++) {
            vec2 neighbor = vec2((float)x, (float)y);
            
            // Random point in that neighbor cell
            vec2 point = hash2(i_st + neighbor);
            
            // Animate the point
            point = vec2(0.5f, 0.5f) + 0.5f * sin(vec4(iTime + 6.2831f * point.x, 
                                                       iTime + 6.2831f * point.y, 0, 0)).xy();
            
            // Vector to that point
            vec2 diff = neighbor + point - f_st;
            
            // Distance
            float dist = length(diff);
            
            // Keep the closest distance
            if(dist < m_dist) m_dist = dist;
        }
    }

    // Color based on distance (inverse)
    vec4 col = vec4(m_dist, m_dist, m_dist, 1.0f);
    
    // Invert for "cells" look
    col = vec4(1.0f, 0.2f, 0.4f, 1.0f) * (1.0f - col.x);
    
    // Add cell centers dots
    col += (1.0f - smoothstep(0.02f, 0.05f, m_dist));

    fragColor = col;
}
