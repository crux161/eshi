#include "glsl_core.h"
#include <math.h>

SHADER_CTX inline float dot3(vec4 a, vec4 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
SHADER_CTX inline float length3(vec4 v) { return sqrtf(dot3(v, v)); }
SHADER_CTX inline vec4 normalize3(vec4 v) { float l = length3(v); return (l==0.0f) ? vec4(0,0,0,0) : v / l; }


SHADER_CTX float map(vec4 p) {  
    
    return length3(p) - 1.0f;
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    
    vec4 ro = vec4(0.0f, 0.0f, -3.0f, 0.0f); 
    vec4 rd = normalize3(vec4(uv.x, uv.y, 1.0f, 0.0f)); 

    
    float t = 0.0f; 
    float d = 0.0f; 
    
    vec4 col = vec4(0,0,0,1); 

    for(int i=0; i<64; i++) {
        vec4 p = ro + rd * t; 
        
        
        p.x += sinf(iTime) * 0.5f;
        
        d = map(p); 
        
        if(d < 0.01f) {
            
            
            vec4 normal = normalize3(p); 
            
            
            vec4 lightDir = normalize3(vec4(sinf(iTime), 1.0f, -1.0f, 0.0f));
            
            
            float diff = dot3(normal, lightDir);
            if(diff < 0.0f) diff = 0.0f; 
            
            
            col = vec4(1.0f, 0.2f, 0.1f, 1.0f) * diff;
            col = col + 0.1f; 
            break; 
        }
        
        t += d; 
        if(t > 10.0f) break; 
    }

    fragColor = col;
}
