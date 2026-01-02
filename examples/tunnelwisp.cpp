#include "../glsl_core.h"
#include <cmath>

using namespace glsl;

/*
 * "Trailing the Twinkling Tunnelwisp"
 * Ported to Eshi/libsumi
 * Original CC0 by @P_Malin (visuals) and Pestis (music)
 */

SHADER_CTX inline vec4 tanh_v(vec4 v) {
    return vec4(::tanhf(v.x), ::tanhf(v.y), ::tanhf(v.z), ::tanhf(v.w));
}

// Optimized exp2f
SHADER_CTX inline float exp2(float x) { return ::exp2f(x); }

// Distance field for gyroid
SHADER_CTX float g(vec4 p, float s) {
    p = p * s;
    vec4 s_val = glsl::sin(p);
    vec4 c_val = glsl::cos(vec4(p.z, p.x, p.w, p.y)); 
    return glsl::abs(dot(s_val, c_val) - 1.0f) / s;
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    float i = 0.0f;
    float d = 0.0f;
    float z = 0.0f;
    float s = 0.0f;
    float T = iTime;
    
    vec4 o = vec4(0.0f);
    vec4 q = vec4(0.0f);
    vec4 p = vec4(0.0f);
    vec4 U = vec4(2.0f, 1.0f, 0.0f, 3.0f); 
    
    vec2 r = iResolution;
    
    for (; i < 79.0f; ++i) {
        z += d + 0.0005f; 
        
        vec3 dir = glsl::normalize(vec3((fragCoord - r * 0.5f).x, (fragCoord - r * 0.5f).y, r.y));
        q = vec4(dir.x * z, dir.y * z, dir.z * z, 0.2f);
        q.z += T / 30.0f; 
        
        s = q.y + 0.1f;
        q.y = glsl::abs(s); 
        
        p = q;
        p.y -= 0.11f;
        
        float angle = -2.0f * p.z;
        float c = glsl::cos(angle);
        float si = glsl::sin(angle);
        float nx = p.x * c - p.y * si;
        float ny = p.x * si + p.y * c;
        p.x = nx; 
        p.y = ny;
        
        p.y -= 0.2f;
        
        d = glsl::abs(g(p, 8.0f) - g(p, 24.0f)) / 4.0f;
        p = glsl::cos(U * 0.7f + q.z * 5.0f) + 1.0f;
        
        float den = glsl::max((s > 0.0f) ? d : d * d * d, 0.0005f);
        o += p * p.w * ((s > 0.0f) ? 1.0f : 0.1f) / den;
    }
    
    o += (glsl::sin(T) * glsl::sin(1.7f * T) * glsl::sin(2.3f * T) + 1.4f)
         * 1000.0f * U / glsl::length(vec2(q.x, q.y));
         
    fragColor = tanh_v(o / 100000.0f);
    fragColor.w = 1.0f;
}

// =============================================================================
// PROCEDURAL AUDIO
// =============================================================================

#ifndef __CUDACC__
extern "C" vec2 mainSound(int s, float t) {
    (void)s;
    vec2 r = vec2(0.0f);
    
    // Original loop logic:
    // for(float i,j;++i<4.;)  -> i runs for 1, 2, 3
    // for(j=0.;++j<5.;)       -> j runs for 1, 2, 3, 4
    
    for(float i = 1.0f; i < 4.0f; i += 1.0f) {
        for(float j = 1.0f; j < 5.0f; j += 1.0f) {
            
            float a = t * j / 32.0f + i / 3.0f;
            float b = glsl::fract(a);
            
            vec2 n = vec2(t, t + 3.0f) + t / j;
            vec2 m = vec2(0.0f);
            
            // Fixed loop: Step (n+=c*=1.02) happens AFTER body in C for-loop
            // for(c=3.;c<4.1;n+=c*=1.02) m+=sin(n*c)/c;
            float c = 3.0f;
            while (c < 4.1f) {
                m += glsl::sin(n * c) / c; // Body
                
                c *= 1.02f; // Step
                n += c;
            }
            
            if(a < 9.0f) {
                float mod_phase = exp2(glsl::mod(a - b, 3.0f) / 6.0f + 8.5f);
                float phase = t * j * i + i + j;
                
                vec2 wave = glsl::sin(m + glsl::sin(t / j / 47.0f) * 4.0f
                                      * glsl::sin(vec2(phase) * mod_phase));
                
                float env = exp2(-b * 12.0f - 1.0f / b + 6.0f - (i + j) / 3.0f);
                
                r += wave * env;
            }
        }
    }
    
    return r;
}
#endif
