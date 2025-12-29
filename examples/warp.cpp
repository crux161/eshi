#include "glsl_core.h"
#include <math.h>

extern SHADER_CTX Sampler2D iChannel0;


SHADER_CTX inline float dot3(vec4 a, vec4 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

SHADER_CTX inline float length3(vec4 v) { return sqrtf(dot3(v, v)); }

SHADER_CTX inline vec4 normalize3(vec4 v) { 
    float l = length3(v); 
    return (l==0.0f) ? vec4(0.0f) : v / l; 
}
SHADER_CTX inline vec4 reflect3(vec4 I, vec4 N) {
    return I - 2.0f * dot3(N, I) * N;
}
SHADER_CTX inline vec2 mod(vec2 x, float y) {
    return vec2(x.x - y * floorf(x.x/y), x.y - y * floorf(x.y/y));
}



SHADER_CTX vec2 W(vec2 p, float iTime) {
    p = (p + 3.0f) * 4.0f;
    float t = iTime / 2.0f;

    for (int i=0; i<3; i++) {
        p += cos(p.yx() * 3.0f + vec2(t, 1.57f)) / 3.0f;
        p += sin(p.yx() + t + vec2(1.57f, 0.0f)) / 2.0f;
        p *= 1.3f; 
    }

    vec2 seed = p + vec2(13.0f, 7.0f);
    float h1 = fract(sinf(seed.x * 12.9898f + seed.y * 78.233f) * 43758.5453f);
    float h2 = fract(sinf(seed.x * 12.9898f + seed.y * 78.233f + 1.0f) * 43758.5453f);
    p += vec2(h1, h2) * 0.03f - 0.015f;

    return mod(p, 2.0f) - 1.0f; 
}

SHADER_CTX float bumpFunc(vec2 p, float iTime) {
    return length(W(p, iTime)) * 0.7071f;
}

SHADER_CTX vec4 smoothFract(vec4 x) { 
    x = fract(x); 
    return vec4(
        fminf(x.x, x.x*(1.0f-x.x)*12.0f),
        fminf(x.y, x.y*(1.0f-x.y)*12.0f),
        fminf(x.z, x.z*(1.0f-x.z)*12.0f),
        0.0f
    ); 
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    vec4 sp = vec4(uv.x, uv.y, 0.0f, 0.0f);
    vec4 rd = normalize3(vec4(uv.x, uv.y, 1.0f, 0.0f));
    vec4 lp = vec4(cosf(iTime)*0.5f, sinf(iTime)*0.2f, -1.0f, 0.0f);
    vec4 sn = vec4(0.0f, 0.0f, -1.0f, 0.0f);

    vec2 eps = vec2(4.0f / iResolution.y, 0.0f);
    
    
    float f = bumpFunc(sp.xy(), iTime);
    float fx = bumpFunc(sp.xy() - eps, iTime); 
    float fy = bumpFunc(sp.xy() - eps.yx(), iTime);

    const float bumpFactor = 0.05f;

    fx = (fx - f) / eps.x;
    fy = (fy - f) / eps.x;

    sn = normalize3(sn + vec4(fx, fy, 0.0f, 0.0f) * bumpFactor);

    vec4 ld = lp - sp;
    float lDist = fmaxf(length3(ld), 0.0001f);
    ld = ld / lDist;

    float atten = 1.0f / (1.0f + lDist * lDist * 0.15f);
    atten *= (f * 0.9f + 0.1f);

    float diff = fmaxf(dot3(sn, ld), 0.0f);
    diff = powf(diff, 4.0f) * 0.66f + powf(diff, 8.0f) * 0.34f;

    vec4 refl = reflect3(ld * -1.0f, sn);
    float spec = powf(fmaxf(dot3(refl, rd * -1.0f), 0.0f), 12.0f);

    vec2 wVal = W(sp.xy(), iTime);
    
    
    
    vec2 texUV = sp.xy() + wVal * 0.125f; 
    vec4 texColRaw = texture(iChannel0, texUV); 
    
    vec4 texCol = texColRaw;
    
    
    texCol = texCol * texCol; 
    
    
    texCol.x = smoothstep(0.05f, 0.75f, powf(texCol.x, 0.75f));
    texCol.y = smoothstep(0.05f, 0.75f, powf(texCol.y, 0.8f));
    texCol.z = smoothstep(0.05f, 0.75f, powf(texCol.z, 0.85f));
    
    
    if (iChannel0.data == nullptr) {
         vec4 warpVec = vec4(wVal.x, wVal.y, wVal.y, 0.0f);
         texCol = smoothFract(warpVec) * 0.1f + 0.2f;
    }

    vec4 col = (texCol * (diff * vec4(1.0f, 0.97f, 0.92f, 1.0f) * 2.0f + 0.5f) + 
                vec4(1.0f, 0.6f, 0.2f, 1.0f) * spec * 2.0f) * atten;

    float ref = fmaxf(dot3(reflect3(rd, sn), vec4(1.0f)), 0.0f);
    col += col * powf(ref, 4.0f) * vec4(0.25f, 0.5f, 1.0f, 1.0f) * 3.0f;

    col.x = sqrtf(clamp(col.x, 0.0f, 1.0f));
    col.y = sqrtf(clamp(col.y, 0.0f, 1.0f));
    col.z = sqrtf(clamp(col.z, 0.0f, 1.0f));
    col.w = 1.0f;

    fragColor = col;
}
