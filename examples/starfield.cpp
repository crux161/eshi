#include "glsl_core.h"
#include <math.h>

// --- Random Hash ---
inline float hash(float n) { 
    float s = sinf(n) * 43758.5453123f;
    return s - floorf(s);
}

void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;
    vec4 col = vec4(0,0,0,1);
    
    // Create multiple layers of stars
    for(int layer=0; layer<4; layer++) {
        
        // Layer variables
        float flayer = (float)layer;
        float speed = 0.5f + flayer * 0.2f;
        float size = 10.0f + flayer * 5.0f;
        float shift = flayer * 135.2f;
        
        // Repeated space logic
        float z = iTime * speed; 
        float gridIndex = floorf(z); // Which "chunk" of space we are in
        float localZ = z - gridIndex; // 0.0 to 1.0 progression
        
        // Generate random star positions for this chunk of space
        for(int i=0; i<10; i++) {
            float seed = (float)i + shift + gridIndex * 13.37f;
            vec2 pos = vec2(hash(seed) - 0.5f, hash(seed + 1.2f) - 0.5f);
            
            // We use (1.0 - localZ) because we want them to come TOWARDS us
            float depth = 1.0f - localZ; 
            vec2 screenPos = pos / depth;
            
            // Distance from pixel UV to star screen position
            vec2 diff = uv - screenPos;
            float distSq = dot(diff, diff);
            
            // Tiny radius for the star, gets smaller as it gets further (depth)
            float radius = 0.0002f / (depth * depth);
            
            if(distSq < radius) {
                // Fade in as they get closer (1.0 - depth)
                col += vec4(1,1,1,1) * (1.0f - depth);
            }
        }
    }
    
    fragColor = col;
}
