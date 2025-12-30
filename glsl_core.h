#pragma once

#ifdef __CUDACC__
    #define SHADER_CTX __host__ __device__
#else
    #define SHADER_CTX 
#endif

#include <cmath>
#include <algorithm>

namespace glsl {
struct vec4; 

struct vec2 { 
    float x, y; 
    SHADER_CTX vec2() : x(0), y(0) {}
    SHADER_CTX vec2(float _x, float _y) : x(_x), y(_y) {}
    
    
    SHADER_CTX explicit vec2(float v) : x(v), y(v) {}
    
    SHADER_CTX vec2 yx() const { return vec2(y,x); } 
    SHADER_CTX vec4 xyyx() const; 
};

struct vec4 { 
    float x, y, z, w; 
    SHADER_CTX vec4() : x(0), y(0), z(0), w(0) {}
    SHADER_CTX vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    
    SHADER_CTX explicit vec4(float v) : x(v), y(v), z(v), w(v) {}

    SHADER_CTX vec2 xy() const { return vec2(x, y); }
    SHADER_CTX vec2 zw() const { return vec2(z, w); }
};


SHADER_CTX inline vec4 vec2::xyyx() const { return vec4(x, y, y, x); }

SHADER_CTX inline vec2 operator *(const vec2 &a, float s) { return vec2(a.x*s, a.y*s); }
SHADER_CTX inline vec2 operator +(const vec2 &a, float s) { return vec2(a.x+s, a.y+s); }
SHADER_CTX inline vec2 operator -(const vec2 &a, float s) { return vec2(a.x-s, a.y-s); }
SHADER_CTX inline vec2 operator *(float s, const vec2 &a) { return a*s; }
SHADER_CTX inline vec2 operator -(const vec2 &a, const vec2 &b) { return vec2(a.x-b.x, a.y-b.y); }
SHADER_CTX inline vec2 operator +(const vec2 &a, const vec2 &b) { return vec2(a.x+b.x, a.y+b.y); }
SHADER_CTX inline vec2 operator *(const vec2 &a, const vec2 &b) { return vec2(a.x*b.x, a.y*b.y); }
SHADER_CTX inline vec2 operator /(const vec2 &a, float s) { return vec2(a.x/s, a.y/s); }

SHADER_CTX inline vec2 &operator +=(vec2 &a, const vec2 &b) { a = a + b; return a; }
SHADER_CTX inline vec2 &operator +=(vec2 &a, float s) { a = a + s; return a; }
SHADER_CTX inline vec2 &operator *=(vec2 &a, float s) { a = a * s; return a; }
SHADER_CTX inline vec2 &operator /=(vec2 &a, float s) { a = a / s; return a; }

SHADER_CTX inline vec4 operator +(const vec4 &a, float s) { return vec4(a.x+s, a.y+s, a.z+s, a.w+s); }
SHADER_CTX inline vec4 operator +(const vec4 &a, const vec4 &b) { return vec4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w); }
SHADER_CTX inline vec4 operator -(const vec4 &a, const vec4 &b) { return vec4(a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w); }
SHADER_CTX inline vec4 operator -(float s, const vec4 &a) { return vec4(s-a.x, s-a.y, s-a.z, s-a.w); }
SHADER_CTX inline vec4 operator *(const vec4 &a, const vec4 &b) { return vec4(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w); }
SHADER_CTX inline vec4 operator *(const vec4 &a, float s) { return vec4(a.x*s, a.y*s, a.z*s, a.w*s); }
SHADER_CTX inline vec4 operator *(float s, const vec4 &a) { return a*s; }
SHADER_CTX inline vec4 operator /(const vec4 &a, float s) { return vec4(a.x/s, a.y/s, a.z/s, a.w/s); }
SHADER_CTX inline vec4 operator /(const vec4 &a, const vec4 &b) { return vec4(a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w); }
SHADER_CTX inline vec4 &operator +=(vec4 &a, const vec4 &b) { a = a + b; return a; }
SHADER_CTX inline vec4 &operator +=(vec4 &a, float s) { a = a + s; return a; }

SHADER_CTX inline vec4 sin(const vec4 &a) { return vec4(sinf(a.x), sinf(a.y), sinf(a.z), sinf(a.w)); } 
SHADER_CTX inline vec4 cos(const vec4 &a) { return vec4(cosf(a.x), cosf(a.y), cosf(a.z), cosf(a.w)); } 
SHADER_CTX inline vec4 exp(const vec4 &a) { return vec4(expf(a.x), expf(a.y), expf(a.z), expf(a.w)); } 
SHADER_CTX inline vec4 tanh(const vec4 &a) { return vec4(tanhf(a.x), tanhf(a.y), tanhf(a.z), tanhf(a.w)); } 

SHADER_CTX inline float dot(const vec2 &a, const vec2 &b) { return a.x*b.x + a.y*b.y; }

SHADER_CTX inline vec2 abs(const vec2 &a) { return vec2(fabsf(a.x), fabsf(a.y)); } 

SHADER_CTX inline vec2 cos(const vec2 &a) { return vec2(cosf(a.x), cosf(a.y)); } 
SHADER_CTX inline vec2 sin(const vec2 &a) { return vec2(sinf(a.x), sinf(a.y)); }

SHADER_CTX inline vec2 floor(const vec2 &a) { return vec2(floorf(a.x), floorf(a.y)); }
SHADER_CTX inline vec4 floor(const vec4 &a) { return vec4(floorf(a.x), floorf(a.y), floorf(a.z), floorf(a.w)); }

SHADER_CTX inline float fract(float x) { return x - floorf(x); }
SHADER_CTX inline vec2 fract(vec2 x) { return x - floor(x); }
SHADER_CTX inline vec4 fract(vec4 x) { return x - floor(x); }

SHADER_CTX inline float clamp(float x, float minVal, float maxVal) { return fmaxf(minVal, fminf(x, maxVal)); }
SHADER_CTX inline float smoothstep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
SHADER_CTX inline float length(vec2 v) { return sqrtf(dot(v, v)); }
SHADER_CTX inline vec4 mix(vec4 x, vec4 y, float a) { return x * (1.0f - a) + y * a; }

struct Sampler2D {
    int w, h;
    vec4* data; 
};

SHADER_CTX inline vec4 texture(const Sampler2D& s, vec2 uv) {
    if (!s.data) return vec4(0.0f);
    
    uv.x = fract(uv.x);
    uv.y = fract(uv.y); 
    
    float x = uv.x * s.w - 0.5f;
    float y = uv.y * s.h - 0.5f;
    
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    
    float u = x - x0;
    float v = y - y0;
    
    auto idx = [&](int xi, int yi) {
        xi = (xi % s.w + s.w) % s.w;
        yi = (yi % s.h + s.h) % s.h;
        return yi * s.w + xi;
    };
    
    vec4 c00 = s.data[idx(x0, y0)];
    vec4 c10 = s.data[idx(x1, y0)];
    vec4 c01 = s.data[idx(x0, y1)];
    vec4 c11 = s.data[idx(x1, y1)];
    
    vec4 top = mix(c00, c10, u);
    vec4 bot = mix(c01, c11, u);
    return mix(top, bot, v);
  }
}
