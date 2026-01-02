#include "../glsl_core.h"

using namespace glsl;

 

const int NUM_STEPS = 32;
const float PI      = 3.141592f;
const float EPSILON = 1e-3f;



#define ITER_GEOMETRY 3
#define ITER_FRAGMENT 5
#define SEA_HEIGHT 0.6f
#define SEA_CHOPPY 4.0f
#define SEA_SPEED 0.8f
#define SEA_FREQ 0.16f
#define SEA_BASE vec3(0.0f, 0.09f, 0.18f)
#define SEA_WATER_COLOR (vec3(0.8f, 0.9f, 0.6f) * 0.6f)


SHADER_CTX inline vec2 mix_v(vec2 x, vec2 y, vec2 a) {
    return vec2(glsl::mix(x.x, y.x, a.x), glsl::mix(x.y, y.y, a.y));
}


SHADER_CTX mat3 fromEuler(vec3 ang) {
    vec2 a1 = vec2(glsl::sin(ang.x), glsl::cos(ang.x));
    vec2 a2 = vec2(glsl::sin(ang.y), glsl::cos(ang.y));
    vec2 a3 = vec2(glsl::sin(ang.z), glsl::cos(ang.z));
    
    vec3 m0 = vec3(a1.y*a3.y+a1.x*a2.x*a3.x, a1.y*a2.x*a3.x+a3.y*a1.x, -a2.y*a3.x);
    vec3 m1 = vec3(-a2.y*a1.x, a1.y*a2.y, a2.x);
    vec3 m2 = vec3(a3.y*a1.x*a2.x+a1.y*a3.x, a1.x*a3.x-a1.y*a3.y*a2.x, a2.y*a3.y);
    
    return mat3(m0, m1, m2);
}



SHADER_CTX float hash(vec2 p) {
    p  = glsl::fract(p * 0.3183099f + 0.1f);
    p *= 17.0f;
    return glsl::fract(p.x * p.y * (p.x + p.y));
}

SHADER_CTX float noise(vec2 p) {
    vec2 i = glsl::floor(p);
    vec2 f = glsl::fract(p);    
    vec2 u = f * f * (3.0f - 2.0f * f);
    return -1.0f + 2.0f * glsl::mix(
                glsl::mix(hash(i + vec2(0.0f, 0.0f)), hash(i + vec2(1.0f, 0.0f)), u.x),
                glsl::mix(hash(i + vec2(0.0f, 1.0f)), hash(i + vec2(1.0f, 1.0f)), u.x), u.y);
}


SHADER_CTX float diffuse(vec3 n, vec3 l, float p) {
    return glsl::pow(dot(n, l) * 0.4f + 0.6f, p);
}

SHADER_CTX float specular(vec3 n, vec3 l, vec3 e, float s) {    
    float nrm = (s + 8.0f) / (PI * 8.0f);
    return glsl::pow(glsl::max(dot(glsl::reflect(e, n), l), 0.0f), s) * nrm;
}


SHADER_CTX vec3 getSkyColor(vec3 e) {
    e.y = (glsl::max(e.y, 0.0f) * 0.8f + 0.2f) * 0.8f;
    return vec3(glsl::pow(1.0f - e.y, 2.0f), 1.0f - e.y, 0.6f + (1.0f - e.y) * 0.4f) * 1.1f;
}


SHADER_CTX float sea_octave(vec2 uv, float choppy) {
    uv += noise(uv);        
    vec2 wv = 1.0f - glsl::abs(glsl::sin(uv));
    vec2 swv = glsl::abs(glsl::cos(uv));    
    wv = mix_v(wv, swv, wv);
    return glsl::pow(1.0f - glsl::pow(wv.x * wv.y, 0.65f), choppy);
}

SHADER_CTX float map(vec3 p, float iTime) {
    float freq = SEA_FREQ;
    float amp = SEA_HEIGHT;
    float choppy = SEA_CHOPPY;
    vec2 uv = vec2(p.x, p.z); 
    uv.x *= 0.75f;
    
    float sea_time = 1.0f + iTime * SEA_SPEED;
    
    float d, h = 0.0f;    
    for(int i = 0; i < ITER_GEOMETRY; i++) {        
        d = sea_octave((uv + sea_time) * freq, choppy);
        d += sea_octave((uv - sea_time) * freq, choppy);
        h += d * amp;        
        
        
        
        
        
        
        float nx = 1.6f * uv.x - 1.2f * uv.y;
        float ny = 1.2f * uv.x + 1.6f * uv.y;
        uv = vec2(nx, ny);
        
        freq *= 1.9f; amp *= 0.22f;
        choppy = glsl::mix(choppy, 1.0f, 0.2f);
    }
    return p.y - h;
}

SHADER_CTX float map_detailed(vec3 p, float iTime) {
    float freq = SEA_FREQ;
    float amp = SEA_HEIGHT;
    float choppy = SEA_CHOPPY;
    vec2 uv = vec2(p.x, p.z); 
    uv.x *= 0.75f;
    
    float sea_time = 1.0f + iTime * SEA_SPEED;
    
    float d, h = 0.0f;    
    for(int i = 0; i < ITER_FRAGMENT; i++) {        
        d = sea_octave((uv + sea_time) * freq, choppy);
        d += sea_octave((uv - sea_time) * freq, choppy);
        h += d * amp;        
        
        
        float nx = 1.6f * uv.x - 1.2f * uv.y;
        float ny = 1.2f * uv.x + 1.6f * uv.y;
        uv = vec2(nx, ny);
        
        freq *= 1.9f; amp *= 0.22f;
        choppy = glsl::mix(choppy, 1.0f, 0.2f);
    }
    return p.y - h;
}

SHADER_CTX vec3 getSeaColor(vec3 p, vec3 n, vec3 l, vec3 eye, vec3 dist) {  
    float fresnel = glsl::clamp(1.0f - dot(n, -eye), 0.0f, 1.0f);
    fresnel = glsl::min(fresnel * fresnel * fresnel, 0.5f);
    
    vec3 reflected = getSkyColor(glsl::reflect(eye, n));    
    vec3 refracted = SEA_BASE + diffuse(n, l, 80.0f) * SEA_WATER_COLOR * 0.12f; 
    
    vec3 color = glsl::mix(refracted, reflected, fresnel);
    
    float atten = glsl::max(1.0f - dot(dist, dist) * 0.001f, 0.0f);
    color += SEA_WATER_COLOR * (p.y - SEA_HEIGHT) * 0.18f * atten;
    
    float dist_sq = dot(dist, dist);
    
    color += specular(n, l, eye, 600.0f * (1.0f / glsl::sqrt(dist_sq)));
    
    return color;
}


SHADER_CTX vec3 getNormal(vec3 p, float eps, float iTime) {
    vec3 n;
    n.y = map_detailed(p, iTime);    
    n.x = map_detailed(vec3(p.x + eps, p.y, p.z), iTime) - n.y;
    n.z = map_detailed(vec3(p.x, p.y, p.z + eps), iTime) - n.y;
    n.y = eps;
    return glsl::normalize(n);
}

SHADER_CTX float heightMapTracing(vec3 ori, vec3 dir, vec3 &p, float iTime) {  
    float tm = 0.0f;
    float tx = 1000.0f;    
    float hx = map(ori + dir * tx, iTime);
    if(hx > 0.0f) {
        p = ori + dir * tx;
        return tx;    
    }
    float hm = map(ori, iTime);    
    for(int i = 0; i < NUM_STEPS; i++) {
        float tmid = glsl::mix(tm, tx, hm / (hm - hx));
        p = ori + dir * tmid;
        float hmid = map(p, iTime);        
        if(hmid < 0.0f) {
            tx = tmid;
            hx = hmid;
        } else {
            tm = tmid;
            hm = hmid;
        }        
        if(glsl::abs(hmid) < EPSILON) break;
    }
    return glsl::mix(tm, tx, hm / (hm - hx));
}

SHADER_CTX vec3 getPixel(vec2 coord, float iTime, vec2 iResolution) {    
    vec2 uv = coord / iResolution;
    uv = uv * 2.0f - 1.0f;
    uv.x *= iResolution.x / iResolution.y;    
        
    
    vec3 ang = vec3(glsl::sin(iTime * 3.0f) * 0.1f, glsl::sin(iTime) * 0.2f + 0.3f, iTime);    
    vec3 ori = vec3(0.0f, 3.5f, iTime * 5.0f);
    vec3 dir = glsl::normalize(vec3(uv.x, uv.y, -2.0f)); 
    dir.z += glsl::length(uv) * 0.14f;
    
    
    mat3 rot = fromEuler(ang);
    vec3 d = glsl::normalize(dir);
    
    dir = vec3( dot(d, rot[0]), dot(d, rot[1]), dot(d, rot[2]) );
    
    
    vec3 p;
    heightMapTracing(ori, dir, p, iTime);
    vec3 dist = p - ori;
    
    float EPSILON_NRM = 0.1f / iResolution.x;
    vec3 n = getNormal(p, dot(dist, dist) * EPSILON_NRM, iTime);
    vec3 light = glsl::normalize(vec3(0.0f, 1.0f, 0.8f)); 
             
    
    return glsl::mix(
        getSkyColor(dir),
        getSeaColor(p, n, light, dir, dist),
        glsl::pow(glsl::smoothstep(0.0f, -0.02f, dir.y), 0.2f));
}


SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 iMouse = vec2(0.0f, 0.0f); 
    float time = iTime * 0.3f + iMouse.x * 0.01f;
    
    vec3 color = getPixel(fragCoord, time, iResolution);
    
    
    fragColor = vec4(glsl::pow(color, vec3(0.65f)), 1.0f);
}
