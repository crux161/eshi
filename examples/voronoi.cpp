#include "../glsl_core.h"
using namespace glsl;

SHADER_CTX inline vec2 hash2(vec2 p) {  
    p = vec2(dot(p, vec2(127.1f, 311.7f)),
             dot(p, vec2(269.5f, 183.3f)));
    // p.xyyx
    vec4 s = sin(vec4(p.x, p.y, p.y, p.x)); 
    
    return vec2(s.x * 43758.5453f - floor(s.x * 43758.5453f), 
                s.y * 43758.5453f - floor(s.y * 43758.5453f));
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.y;
    uv = uv * 5.0f;
    
    vec2 i_st = vec2(floor(uv.x), floor(uv.y));
    vec2 f_st = uv - i_st;

    float m_dist = 1.0f;  

    for (int y= -1; y <= 1; y++) {
        for (int x= -1; x <= 1; x++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = hash2(i_st + neighbor);
            
            vec4 trig = sin(vec4(iTime + 6.2831f * point.x, iTime + 6.2831f * point.y, 0, 0));
            // .xy()
            point = vec2(0.5f, 0.5f) + 0.5f * vec2(trig.x, trig.y);
            
            vec2 diff = neighbor + point - f_st;
            float dist = length(diff);
            
            if(dist < m_dist) m_dist = dist;
        }
    }

    vec4 col = vec4(m_dist, m_dist, m_dist, 1.0f);
    col = vec4(1.0f, 0.2f, 0.4f, 1.0f) * (1.0f - col.x);
    col += (1.0f - smoothstep(0.02f, 0.05f, m_dist));

    fragColor = col;
}
