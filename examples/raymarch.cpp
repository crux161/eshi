#include "glsl_core.h"
#include <math.h>

SHADER_CTX inline float dot3(vec4 a, vec4 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
SHADER_CTX inline float length3(vec4 v) { return sqrtf(dot3(v, v)); }
SHADER_CTX inline vec4 normalize3(vec4 v) { float l = length3(v); return (l==0.0f) ? vec4(0,0,0,0) : v / l; }

// Signed Distance Function for a Sphere
SHADER_CTX float map(vec4 p) {  // <--- ADDED SHADER_CTX
    // Sphere radius 1.0 at (0,0,0)
    return length3(p) - 1.0f;
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    // 1. Setup Coordinates
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    // 2. Camera Setup
    vec4 ro = vec4(0.0f, 0.0f, -3.0f, 0.0f); // Ray Origin (Camera pos)
    vec4 rd = normalize3(vec4(uv.x, uv.y, 1.0f, 0.0f)); // Ray Direction

    // 3. Raymarching Loop
    float t = 0.0f; // Distance traveled
    float d = 0.0f; // Distance to object
    
    vec4 col = vec4(0,0,0,1); // Default black background

    for(int i=0; i<64; i++) {
        vec4 p = ro + rd * t; // Current position
        
        // Rotate the sphere position slightly just to prove it's 3D
        p.x += sinf(iTime) * 0.5f;
        
        d = map(p); // Check distance to sphere
        
        if(d < 0.01f) {
            // Hit! Calculate lighting
            // Normal is roughly the position on a unit sphere
            vec4 normal = normalize3(p); 
            
            // Light source direction
            vec4 lightDir = normalize3(vec4(sinf(iTime), 1.0f, -1.0f, 0.0f));
            
            // Diffuse lighting (dot product of normal and light)
            float diff = dot3(normal, lightDir);
            if(diff < 0.0f) diff = 0.0f; // Clamp to 0
            
            // Object Color (Red-ish) + Light
            col = vec4(1.0f, 0.2f, 0.1f, 1.0f) * diff;
            col = col + 0.1f; // Ambient light
            break; 
        }
        
        t += d; // March forward
        if(t > 10.0f) break; // Too far, stop
    }

    fragColor = col;
}
