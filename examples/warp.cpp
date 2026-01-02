#include "../glsl_core.h"
using namespace glsl;


#ifndef __CUDACC__
    extern Sampler2D iChannel0;
#else
    
    
    extern __device__ Sampler2D iChannel0;
#endif


SHADER_CTX inline float dot3(vec4 a, vec4 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
SHADER_CTX inline float length3(vec4 v) { return glsl::sqrt(dot3(v, v)); }
SHADER_CTX inline vec4 normalize3(vec4 v) { float l = length3(v); return (l==0.0f) ? vec4(0.0f) : v / l; }
SHADER_CTX inline vec4 reflect3(vec4 I, vec4 N) { return I - 2.0f * dot3(N, I) * N; }


SHADER_CTX inline vec2 my_mod(vec2 x, float y) { return vec2(x.x - y * glsl::floor(x.x/y), x.y - y * glsl::floor(x.y/y)); }

SHADER_CTX vec2 W(vec2 p, float iTime) {
    p = (p + 3.0f) * 4.0f;
    float t = iTime / 2.0f;

    for (int i=0; i<3; i++) {
        p += glsl::cos(vec2(p.y, p.x) * 3.0f + vec2(t, 1.57f)) / 3.0f;
        p += glsl::sin(vec2(p.y, p.x) + t + vec2(1.57f, 0.0f)) / 2.0f;
        p *= 1.3f; 
    }

    vec2 seed = p + vec2(13.0f, 7.0f);
    float h1 = glsl::fract(glsl::sin(seed.x * 12.9898f + seed.y * 78.233f) * 43758.5453f);
    float h2 = glsl::fract(glsl::sin(seed.x * 12.9898f + seed.y * 78.233f + 1.0f) * 43758.5453f);
    p += vec2(h1, h2) * 0.03f - 0.015f;

    return my_mod(p, 2.0f) - 1.0f; 
}

SHADER_CTX float bumpFunc(vec2 p, float iTime) {
    return glsl::length(W(p, iTime)) * 0.7071f;
}

SHADER_CTX vec4 smoothFract(vec4 x) { 
    x = glsl::fract(x); 
    return vec4(
        glsl::min(x.x, x.x*(1.0f-x.x)*12.0f),
        glsl::min(x.y, x.y*(1.0f-x.y)*12.0f),
        glsl::min(x.z, x.z*(1.0f-x.z)*12.0f),
        0.0f
    ); 
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    vec4 sp = vec4(uv.x, uv.y, 0.0f, 0.0f);
    vec4 rd = normalize3(vec4(uv.x, uv.y, 1.0f, 0.0f));
    vec4 lp = vec4(glsl::cos(iTime)*0.5f, glsl::sin(iTime)*0.2f, -1.0f, 0.0f);
    vec4 sn = vec4(0.0f, 0.0f, -1.0f, 0.0f);

    vec2 eps = vec2(4.0f / iResolution.y, 0.0f);
    
    vec2 sp_xy = vec2(sp.x, sp.y);
    float f = bumpFunc(sp_xy, iTime);
    float fx = bumpFunc(sp_xy - eps, iTime); 
    float fy = bumpFunc(sp_xy - vec2(eps.y, eps.x), iTime);

    const float bumpFactor = 0.05f;

    fx = (fx - f) / eps.x;
    fy = (fy - f) / eps.x;

    sn = normalize3(sn + vec4(fx, fy, 0.0f, 0.0f) * bumpFactor);

    vec4 ld = lp - sp;
    float lDist = glsl::max(length3(ld), 0.0001f);
    ld = ld / lDist;

    float atten = 1.0f / (1.0f + lDist * lDist * 0.15f);
    atten *= (f * 0.9f + 0.1f);

    float diff = glsl::max(dot3(sn, ld), 0.0f);
    diff = glsl::pow(diff, 4.0f) * 0.66f + glsl::pow(diff, 8.0f) * 0.34f;

    vec4 refl = reflect3(ld * -1.0f, sn);
    float spec = glsl::pow(glsl::max(dot3(refl, rd * -1.0f), 0.0f), 12.0f);

    vec2 wVal = W(sp_xy, iTime);
    vec2 texUV = sp_xy + wVal * 0.125f; 
    
    
    #ifdef __CUDACC__
    vec4 texColRaw = texture(::iChannel0, texUV); 
    #else
    vec4 texColRaw = texture(iChannel0, texUV); 
    #endif

    vec4 texCol = texColRaw;
    texCol = texCol * texCol; 
    
    texCol.x = glsl::smoothstep(0.05f, 0.75f, glsl::pow(texCol.x, 0.75f));
    texCol.y = glsl::smoothstep(0.05f, 0.75f, glsl::pow(texCol.y, 0.8f));
    texCol.z = glsl::smoothstep(0.05f, 0.75f, glsl::pow(texCol.z, 0.85f));
    
    
    #ifndef __CUDACC__
    if (iChannel0.data == nullptr) {
         vec4 warpVec = vec4(wVal.x, wVal.y, wVal.y, 0.0f);
         texCol = smoothFract(warpVec) * 0.1f + 0.2f;
    }
    #endif

    vec4 col = (texCol * (diff * vec4(1.0f, 0.97f, 0.92f, 1.0f) * 2.0f + 0.5f) + 
                vec4(1.0f, 0.6f, 0.2f, 1.0f) * spec * 2.0f) * atten;

    float ref = glsl::max(dot3(reflect3(rd, sn), vec4(1.0f)), 0.0f);
    col += col * glsl::pow(ref, 4.0f) * vec4(0.25f, 0.5f, 1.0f, 1.0f) * 3.0f;

    col.x = glsl::sqrt(glsl::clamp(col.x, 0.0f, 1.0f));
    col.y = glsl::sqrt(glsl::clamp(col.y, 0.0f, 1.0f));
    col.z = glsl::sqrt(glsl::clamp(col.z, 0.0f, 1.0f));
    col.w = 1.0f;

    fragColor = col;
}
