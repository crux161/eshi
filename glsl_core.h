#pragma once
#include <cmath>
#include <algorithm>
#include <iostream>

#ifdef __CUDACC__
    #define SHADER_CTX __device__ __host__
#else
    #define SHADER_CTX
#endif

namespace glsl {

    // --- Import Standard Scalars ---
    using std::abs;
    using std::min;
    using std::max;
    using std::pow;
    using std::sqrt;
    using std::sin;
    using std::cos;
    using std::floor;
    using std::ceil;
    using std::exp;
    using std::log;
    using std::tanh;

    // --- Forward Declarations ---
    struct vec2;
    struct vec3;
    struct vec4;

    // --- Basic Types ---
    struct vec2 {
        float x, y;
        SHADER_CTX vec2() : x(0), y(0) {}
        SHADER_CTX vec2(float v) : x(v), y(v) {}
        SHADER_CTX vec2(float _x, float _y) : x(_x), y(_y) {}
        
        SHADER_CTX vec2 operator+(const vec2& r) const { return vec2(x+r.x, y+r.y); }
        SHADER_CTX vec2 operator-(const vec2& r) const { return vec2(x-r.x, y-r.y); }
        SHADER_CTX vec2 operator*(float s) const { return vec2(x*s, y*s); }
        SHADER_CTX vec2 operator*(const vec2& r) const { return vec2(x*r.x, y*r.y); }
        SHADER_CTX vec2 operator/(float s) const { return vec2(x/s, y/s); }
        SHADER_CTX vec2 operator/(const vec2& r) const { return vec2(x/r.x, y/r.y); }
        
        SHADER_CTX vec2& operator+=(const vec2& r) { x+=r.x; y+=r.y; return *this; }
        SHADER_CTX vec2& operator-=(const vec2& r) { x-=r.x; y-=r.y; return *this; }
        SHADER_CTX vec2& operator*=(const vec2& r) { x*=r.x; y*=r.y; return *this; }
        SHADER_CTX vec2& operator+=(float s) { x+=s; y+=s; return *this; }
        SHADER_CTX vec2& operator-=(float s) { x-=s; y-=s; return *this; }
        SHADER_CTX vec2& operator*=(float s) { x*=s; y*=s; return *this; }
        SHADER_CTX vec2& operator/=(float s) { x/=s; y/=s; return *this; }
    };

    struct vec3 {
        float x, y, z;
        SHADER_CTX vec3() : x(0), y(0), z(0) {}
        SHADER_CTX vec3(float v) : x(v), y(v), z(v) {}
        SHADER_CTX vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
        
        SHADER_CTX vec3 operator+(const vec3& r) const { return vec3(x+r.x, y+r.y, z+r.z); }
        SHADER_CTX vec3 operator-(const vec3& r) const { return vec3(x-r.x, y-r.y, z-r.z); }
        SHADER_CTX vec3 operator*(float s) const { return vec3(x*s, y*s, z*s); }
        SHADER_CTX vec3 operator*(const vec3& r) const { return vec3(x*r.x, y*r.y, z*r.z); } 
        SHADER_CTX vec3 operator/(float s) const { return vec3(x/s, y/s, z/s); }
        SHADER_CTX vec3 operator/(const vec3& r) const { return vec3(x/r.x, y/r.y, z/r.z); }
        
        SHADER_CTX vec3& operator+=(const vec3& r) { x+=r.x; y+=r.y; z+=r.z; return *this; }
        SHADER_CTX vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
        SHADER_CTX vec3& operator+=(float s) { x+=s; y+=s; z+=s; return *this; }
    };

    struct vec4 {
        float x, y, z, w;
        SHADER_CTX vec4() : x(0), y(0), z(0), w(0) {}
        SHADER_CTX vec4(float v) : x(v), y(v), z(v), w(v) {}
        SHADER_CTX vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
        SHADER_CTX vec4(const vec3& v, float _w) : x(v.x), y(v.y), z(v.z), w(_w) {}
        
        SHADER_CTX vec4 operator*(float s) const { return vec4(x*s, y*s, z*s, w*s); }
        SHADER_CTX vec4 operator*(const vec4& r) const { return vec4(x*r.x, y*r.y, z*r.z, w*r.w); }
        SHADER_CTX vec4 operator/(float s) const { return vec4(x/s, y/s, z/s, w/s); } 
        SHADER_CTX vec4 operator/(const vec4& r) const { return vec4(x/r.x, y/r.y, z/r.z, w/r.w); } 
        SHADER_CTX vec4 operator+(const vec4& r) const { return vec4(x+r.x, y+r.y, z+r.z, w+r.w); }
        SHADER_CTX vec4 operator-(const vec4& r) const { return vec4(x-r.x, y-r.y, z-r.z, w-r.w); }
        
        SHADER_CTX vec4& operator+=(const vec4& r) { x+=r.x; y+=r.y; z+=r.z; w+=r.w; return *this; }
        SHADER_CTX vec4& operator*=(float s) { x*=s; y*=s; z*=s; w*=s; return *this; }
    };

    struct Sampler2D {
        int w, h;
        vec4* data;
    };

    struct InputState {
        bool up, down, w, s, space;
        float dt, time;
    };

    struct GameData {
        vec2 paddleL;
        vec2 paddleR;
        vec2 ballPos;
        vec2 ballVel;
        float hitTimer;
    };

    // --- Global Operators ---
    SHADER_CTX inline vec2 operator+(float s, const vec2& v) { return vec2(s+v.x, s+v.y); }
    SHADER_CTX inline vec3 operator+(float s, const vec3& v) { return vec3(s+v.x, s+v.y, s+v.z); }
    SHADER_CTX inline vec4 operator+(float s, const vec4& v) { return vec4(s+v.x, s+v.y, s+v.z, s+v.w); }

    SHADER_CTX inline vec2 operator-(float s, const vec2& v) { return vec2(s-v.x, s-v.y); }
    SHADER_CTX inline vec3 operator-(float s, const vec3& v) { return vec3(s-v.x, s-v.y, s-v.z); }
    SHADER_CTX inline vec4 operator-(float s, const vec4& v) { return vec4(s-v.x, s-v.y, s-v.z, s-v.w); }

    SHADER_CTX inline vec2 operator*(float s, const vec2& v) { return v * s; }
    SHADER_CTX inline vec3 operator*(float s, const vec3& v) { return v * s; }
    SHADER_CTX inline vec4 operator*(float s, const vec4& v) { return v * s; }

    SHADER_CTX inline vec2 operator/(float s, const vec2& v) { return vec2(s/v.x, s/v.y); }
    SHADER_CTX inline vec3 operator/(float s, const vec3& v) { return vec3(s/v.x, s/v.y, s/v.z); }
    SHADER_CTX inline vec4 operator/(float s, const vec4& v) { return vec4(s/v.x, s/v.y, s/v.z, s/v.w); }

    // --- Helper Functions ---
    SHADER_CTX inline float fract(float x) { return x - ::floorf(x); }
    SHADER_CTX inline float clamp(float x, float minVal, float maxVal) { return ::fminf(::fmaxf(x, minVal), maxVal); }
    SHADER_CTX inline float smoothstep(float e0, float e1, float x) {
        float t = clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
    SHADER_CTX inline float mix(float x, float y, float a) { return x * (1.0f - a) + y * a; }
    SHADER_CTX inline float length(float a) { return ::fabsf(a); }

    // --- Vector Math ---
    
    // Length & Dot
    SHADER_CTX inline float length(const vec2& v) { return sqrtf(v.x*v.x + v.y*v.y); }
    SHADER_CTX inline float length(const vec3& v) { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
    SHADER_CTX inline float length(const vec4& v) { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z + v.w*v.w); }
    
    SHADER_CTX inline float dot(const vec2& a, const vec2& b) { return a.x*b.x + a.y*b.y; }
    SHADER_CTX inline float dot(const vec3& a, const vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
    SHADER_CTX inline float dot(const vec4& a, const vec4& b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

    // Common Ops
    SHADER_CTX inline vec2 abs(const vec2 &a) { return vec2(::fabsf(a.x), ::fabsf(a.y)); }
    SHADER_CTX inline vec2 max(const vec2 &a, float b) { return vec2(::fmaxf(a.x,b), ::fmaxf(a.y,b)); }
    SHADER_CTX inline vec2 min(const vec2 &a, float b) { return vec2(::fminf(a.x,b), ::fminf(a.y,b)); }
    SHADER_CTX inline vec2 floor(const vec2 &a) { return vec2(::floorf(a.x), ::floorf(a.y)); }
    
    // --- FRACT (The missing piece for Warp!) ---
    SHADER_CTX inline vec2 fract(const vec2 &a) { return vec2(fract(a.x), fract(a.y)); }
    SHADER_CTX inline vec3 fract(const vec3 &a) { return vec3(fract(a.x), fract(a.y), fract(a.z)); }
    SHADER_CTX inline vec4 fract(const vec4 &a) { return vec4(fract(a.x), fract(a.y), fract(a.z), fract(a.w)); }
    
    // Mix
    SHADER_CTX inline vec3 mix(const vec3 &x, const vec3 &y, float a) { return vec3(mix(x.x, y.x, a), mix(x.y, y.y, a), mix(x.z, y.z, a)); }
    SHADER_CTX inline vec4 mix(const vec4 &x, const vec4 &y, float a) { return vec4(mix(x.x, y.x, a), mix(x.y, y.y, a), mix(x.z, y.z, a), mix(x.w, y.w, a)); }

    // Trig & Exp
    SHADER_CTX inline vec2 sin(const vec2 &a) { return vec2(::sinf(a.x), ::sinf(a.y)); }
    SHADER_CTX inline vec4 sin(const vec4 &a) { return vec4(::sinf(a.x), ::sinf(a.y), ::sinf(a.z), ::sinf(a.w)); }
    SHADER_CTX inline vec2 cos(const vec2 &a) { return vec2(::cosf(a.x), ::cosf(a.y)); }
    SHADER_CTX inline vec4 cos(const vec4 &a) { return vec4(::cosf(a.x), ::cosf(a.y), ::cosf(a.z), ::cosf(a.w)); }
    SHADER_CTX inline vec3 sqrt(const vec3 &a) { return vec3(::sqrtf(a.x), ::sqrtf(a.y), ::sqrtf(a.z)); }
    SHADER_CTX inline vec4 exp(const vec4 &a) { return vec4(::expf(a.x), ::expf(a.y), ::expf(a.z), ::expf(a.w)); }
    SHADER_CTX inline vec4 tanh(const vec4 &a) { return vec4(::tanhf(a.x), ::tanhf(a.y), ::tanhf(a.z), ::tanhf(a.w)); }
    
    // Texture
    SHADER_CTX inline vec4 texture(const Sampler2D& s, const vec2& uv) {
        if (!s.data) return vec4(0,0,0,1);
        int tx = (int)(uv.x * s.w) % s.w;
        int ty = (int)(uv.y * s.h) % s.h;
        if (tx < 0) tx += s.w; 
        if (ty < 0) ty += s.h;
        return s.data[ty * s.w + tx];
    }
}
