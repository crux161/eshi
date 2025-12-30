#include "../glsl_core.h"
#include <math.h>

using namespace glsl;

SHADER_CTX inline float hash(float n) {  
    float s = sinf(n) * 43758.5453123f;
    return s - floorf(s);
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;
    vec4 col = vec4(0,0,0,1);
    
    
    for(int layer=0; layer<4; layer++) {
        
        
        float flayer = float(layer);
        float speed = 0.5f + flayer * 0.2f;
        float size = 10.0f + flayer * 5.0f;
        float shift = flayer * 135.2f;
        
        
        float z = iTime * speed; 
        float gridIndex = floorf(z); 
        float localZ = z - gridIndex; 
        
        
        for(int i=0; i<10; i++) {
            float seed = float(i) + shift + gridIndex * 13.37f;
            vec2 pos = vec2(hash(seed) - 0.5f, hash(seed + 1.2f) - 0.5f);
            
            
            float depth = 1.0f - localZ; 
            vec2 screenPos = pos / depth;
            
            
            vec2 diff = uv - screenPos;
            float distSq = dot(diff, diff);
            
            
            float radius = 0.0002f / (depth * depth);
            
            if(distSq < radius) {
                
                col += vec4(1,1,1,1) * (1.0f - depth);
            }
        }
    }
    
    fragColor = col;
}
