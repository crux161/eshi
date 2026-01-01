#include "../glsl_core.h"
#include <algorithm> // For std::min/max

using namespace glsl;

// =========================================================================
// 🛠️ COMPATIBILITY LAYER
// (Features used by IQ's code that we haven't added to glsl_core.h yet)
// =========================================================================

// 1. Missing Matrix (mat3)
struct mat3 {
    vec3 cols[3];
    mat3() {}
    mat3(vec3 a, vec3 b, vec3 c) { cols[0]=a; cols[1]=b; cols[2]=c; }
};

// 2. Matrix-Vector Multiplication for mat3
vec3 operator*(const mat3 &m, const vec3 &v) {
    return vec3(
        m.cols[0].x*v.x + m.cols[1].x*v.y + m.cols[2].x*v.z,
        m.cols[0].y*v.x + m.cols[1].y*v.y + m.cols[2].y*v.z,
        m.cols[0].z*v.x + m.cols[1].z*v.y + m.cols[2].z*v.z
    );
}

// 3. Missing Math Intrinsics
inline float sign(float x) { return (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f); }
inline vec2 sign(vec2 v) { return vec2(sign(v.x), sign(v.y)); }
inline float dot2(vec2 v) { return dot(v,v); }
inline float dot2(vec3 v) { return dot(v,v); }
inline float ndot(vec2 a, vec2 b ) { return a.x*b.x - a.y*b.y; }

// 4. Missing Vector Min/Max Overloads
inline vec3 min(vec3 a, float b) { return vec3(fminf(a.x,b), fminf(a.y,b), fminf(a.z,b)); }
inline vec3 max(vec3 a, float b) { return vec3(fmaxf(a.x,b), fmaxf(a.y,b), fmaxf(a.z,b)); }
inline vec3 min(vec3 a, vec3 b) { return vec3(fminf(a.x,b.x), fminf(a.y,b.y), fminf(a.z,b.z)); }
inline vec3 max(vec3 a, vec3 b) { return vec3(fmaxf(a.x,b.x), fmaxf(a.y,b.y), fmaxf(a.z,b.z)); }
inline vec2 min(vec2 a, vec2 b) { return vec2(fminf(a.x,b.x), fminf(a.y,b.y)); }
inline vec2 max(vec2 a, vec2 b) { return vec2(fmaxf(a.x,b.x), fmaxf(a.y,b.y)); }
inline float min(float a, float b) { return fminf(a, b); }
inline float max(float a, float b) { return fmaxf(a, b); }

// =========================================================================
// 🎨 INIGO QUILEZ SDF PRIMITIVES
// (Ported from GLSL to Eshi C++)
// =========================================================================

#define AA 1 // Antialiasing level (1 = Off, 2+ = On)

float sdPlane( vec3 p ) {
    return p.y;
}

float sdSphere( vec3 p, float s ) {
    return length(p)-s;
}

float sdBox( vec3 p, vec3 b ) {
    vec3 d = abs(p) - b;
    return min(max(d.x,max(d.y,d.z)),0.0f) + length(max(d,0.0f));
}

float sdBoxFrame( vec3 p, vec3 b, float e ) {
    p = abs(p)-b;
    vec3 q = abs(p+e)-e;
    return min(min(
        length(max(vec3(p.x,q.y,q.z),0.0f))+min(max(p.x,max(q.y,q.z)),0.0f),
        length(max(vec3(q.x,p.y,q.z),0.0f))+min(max(q.x,max(p.y,q.z)),0.0f)),
        length(max(vec3(q.x,q.y,p.z),0.0f))+min(max(q.x,max(q.y,p.z)),0.0f));
}

float sdEllipsoid( vec3 p, vec3 r ) {
    float k0 = length(p/r);
    float k1 = length(p/(r*r));
    return k0*(k0-1.0f)/k1;
}

float sdTorus( vec3 p, vec2 t ) {
    return length( vec2(length(p.xz())-t.x,p.y) )-t.y;
}

float sdCappedTorus( vec3 p, vec2 sc, float ra, float rb) {
    p.x = fabsf(p.x);
    float k = (sc.y*p.x>sc.x*p.y) ? dot(p.xy(),sc) : length(p.xy());
    return sqrtf( dot(p,p) + ra*ra - 2.0f*ra*k ) - rb;
}

float sdHexPrism( vec3 p, vec2 h ) {
    const vec3 k = vec3(-0.8660254f, 0.5f, 0.57735f);
    p = abs(p);
    p.x -= 2.0f*min(dot(p.xy(), k.xy()), 0.0f)*k.x;
    p.y -= 2.0f*min(dot(p.xy(), k.xy()), 0.0f)*k.y;
    vec2 d = vec2(
       length(p.xy() - vec2(clamp(p.x, -k.z*h.x, k.z*h.x), h.x))*sign(p.y - h.x),
       p.z-h.y );
    return min(max(d.x,d.y),0.0f) + length(max(d,0.0f));
}

float sdOctogonPrism( vec3 p, float r, float h ) {
  const vec3 k = vec3(-0.9238795325f, 0.3826834323f, 0.4142135623f); 
  p = abs(p);
  p.x -= 2.0f*min(dot(vec2(k.x,k.y),p.xy()),0.0f)*k.x;
  p.y -= 2.0f*min(dot(vec2(k.x,k.y),p.xy()),0.0f)*k.y;
  p.x -= 2.0f*min(dot(vec2(-k.x,k.y),p.xy()),0.0f)*-k.x;
  p.y -= 2.0f*min(dot(vec2(-k.x,k.y),p.xy()),0.0f)*k.y;
  p.x -= clamp(p.x, -k.z*r, k.z*r);
  p.y -= r;
  vec2 d = vec2( length(p.xy())*sign(p.y), p.z-h );
  return min(max(d.x,d.y),0.0f) + length(max(d,0.0f));
}

float sdCapsule( vec3 p, vec3 a, vec3 b, float r ) {
    vec3 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0f, 1.0f );
    return length( pa - ba*h ) - r;
}

float sdRoundCone( vec3 p, float r1, float r2, float h ) {
    vec2 q = vec2( length(p.xz()), p.y );
    float b = (r1-r2)/h;
    float a = sqrtf(1.0f-b*b);
    float k = dot(q,vec2(-b,a));
    if( k < 0.0f ) return length(q) - r1;
    if( k > a*h ) return length(q-vec2(0.0f,h)) - r2;
    return dot(q, vec2(a,b) ) - r1;
}

float sdTriPrism( vec3 p, vec2 h ) {
    const float k = sqrtf(3.0f);
    h.x *= 0.5f*k;
    p.x /= h.x; p.y /= h.x;
    p.x = fabsf(p.x) - 1.0f;
    p.y = p.y + 1.0f/k;
    if( p.x+k*p.y>0.0f ) {
        float px = p.x; float py = p.y;
        p.x = px-k*py; p.y = -k*px-py;
        p.x /= 2.0f; p.y /= 2.0f;
    }
    p.x -= clamp( p.x, -2.0f, 0.0f );
    float d1 = length(p.xy())*sign(-p.y)*h.x;
    float d2 = fabsf(p.z)-h.y;
    return length(max(vec2(d1,d2),0.0f)) + min(max(d1,d2), 0.0f);
}

float sdCylinder( vec3 p, vec2 h ) {
    vec2 d = abs(vec2(length(p.xz()),p.y)) - h;
    return min(max(d.x,d.y),0.0f) + length(max(d,0.0f));
}

float sdCylinder(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a;
    vec3 ba = b - a;
    float baba = dot(ba,ba);
    float paba = dot(pa,ba);
    float x = length(pa*baba-ba*paba) - r*baba;
    float y = fabsf(paba-baba*0.5f)-baba*0.5f;
    float x2 = x*x;
    float y2 = y*y*baba;
    float d = (max(x,y)<0.0f)?-min(x2,y2):(((x>0.0f)?x2:0.0f)+((y>0.0f)?y2:0.0f));
    return sign(d)*sqrtf(fabsf(d))/baba;
}

float sdCone( vec3 p, vec2 c, float h ) {
    vec2 q = h*vec2(c.x,-c.y)/c.y;
    vec2 w = vec2( length(p.xz()), p.y );
    vec2 a = w - q*clamp( dot(w,q)/dot(q,q), 0.0f, 1.0f );
    vec2 b = w - q*vec2( clamp( w.x/q.x, 0.0f, 1.0f ), 1.0f );
    float k = sign( q.y );
    float d = min(dot( a, a ),dot(b, b));
    float s = max( k*(w.x*q.y-w.y*q.x),k*(w.y-q.y)  );
    return sqrtf(d)*sign(s);
}

float sdCappedCone( vec3 p, float h, float r1, float r2 ) {
    vec2 q = vec2( length(p.xz()), p.y );
    vec2 k1 = vec2(r2,h);
    vec2 k2 = vec2(r2-r1,2.0f*h);
    vec2 ca = vec2(q.x-min(q.x,(q.y < 0.0f)?r1:r2), fabsf(q.y)-h);
    vec2 cb = q - k1 + k2*clamp( dot(k1-q,k2)/dot2(k2), 0.0f, 1.0f );
    float s = (cb.x < 0.0f && ca.y < 0.0f) ? -1.0f : 1.0f;
    return s*sqrtf( min(dot2(ca),dot2(cb)) );
}

float sdOctahedron(vec3 p, float s) {
    p = abs(p);
    float m = p.x + p.y + p.z - s;
    vec3 q;
         if( 3.0f*p.x < m ) q = p.xyz();
    else if( 3.0f*p.y < m ) q = vec3(p.y, p.z, p.x);
    else if( 3.0f*p.z < m ) q = vec3(p.z, p.x, p.y);
    else return m*0.57735027f;
    float k = clamp(0.5f*(q.z-q.y+s),0.0f,s); 
    return length(vec3(q.x,q.y-s+k,q.z-k)); 
}

float sdPyramid( vec3 p, float h ) {
    float m2 = h*h + 0.25f;
    p.x = fabsf(p.x); p.z = fabsf(p.z);
    if (p.z > p.x) { float t=p.z; p.z=p.x; p.x=t; } // p.xz = (p.z>p.x) ? p.zx : p.xz;
    p.x -= 0.5f; p.z -= 0.5f;
    vec3 q = vec3( p.z, h*p.y - 0.5f*p.x, h*p.x + 0.5f*p.y);
    float s = max(-q.x,0.0f);
    float t = clamp( (q.y-0.5f*p.z)/(m2+0.25f), 0.0f, 1.0f );
    float a = m2*(q.x+s)*(q.x+s) + q.y*q.y;
    float b = m2*(q.x+0.5f*t)*(q.x+0.5f*t) + (q.y-m2*t)*(q.y-m2*t);
    float d2 = min(q.y,-q.x*m2-q.y*0.5f) > 0.0f ? 0.0f : min(a,b);
    return sqrtf( (d2+q.z*q.z)/m2 ) * sign(max(q.z,-p.y));
}

float sdRhombus(vec3 p, float la, float lb, float h, float ra) {
    p = abs(p);
    vec2 b = vec2(la,lb);
    float f = clamp( (ndot(b,b-2.0f*p.xz()))/dot(b,b), -1.0f, 1.0f );
    vec2 q = vec2(length(p.xz()-0.5f*b*vec2(1.0f-f,1.0f+f))*sign(p.x*b.y+p.z*b.x-b.x*b.y)-ra, p.y-h);
    return min(max(q.x,q.y),0.0f) + length(max(q,0.0f));
}

// ------------------------------------------------------------------
// SCENE MAPPING
// ------------------------------------------------------------------

vec2 opU( vec2 d1, vec2 d2 ) {
    return (d1.x<d2.x) ? d1 : d2;
}

vec2 map( vec3 pos, int iFrame ) {
    vec2 res = vec2( pos.y, 0.0f );
    
    // bounding box check (optimization)
    if( sdBox( pos-vec3(-2.0,0.3,0.25),vec3(0.3,0.3,1.0) ) < res.x ) {
      res = opU( res, vec2( sdSphere(    pos-vec3(-2.0,0.25, 0.0), 0.25 ), 26.9 ) );
      res = opU( res, vec2( sdRhombus(  (pos-vec3(-2.0,0.25, 1.0)).yzx(), 0.15, 0.25, 0.04, 0.08 ),17.0 ) );
    }

    if( sdBox( pos-vec3(0.0,0.3,-1.0),vec3(0.35,0.3,2.5) )<res.x ) {
      res = opU( res, vec2( sdCappedTorus((pos-vec3( 0.0,0.30, 1.0))*vec3(1,-1,1), vec2(0.866025,-0.5), 0.25, 0.05), 25.0) );
      res = opU( res, vec2( sdBoxFrame(    pos-vec3( 0.0,0.25, 0.0), vec3(0.3,0.25,0.2), 0.025 ), 16.9 ) );
      res = opU( res, vec2( sdCone(        pos-vec3( 0.0,0.45,-1.0), vec2(0.6,0.8),0.45 ), 55.0 ) );
      res = opU( res, vec2( sdCappedCone(  pos-vec3( 0.0,0.25,-2.0), 0.25, 0.25, 0.1 ), 13.67 ) );
    }

    if( sdBox( pos-vec3(1.0,0.3,-1.0),vec3(0.35,0.3,2.5) )<res.x ) {
      res = opU( res, vec2( sdTorus(       (pos-vec3( 1.0,0.30, 1.0)).yzx(), vec2(0.25,0.05) ), 7.1 ) );
      res = opU( res, vec2( sdBox(          pos-vec3( 1.0,0.25, 0.0), vec3(0.3,0.25,0.1) ), 3.0 ) );
      res = opU( res, vec2( sdCapsule(      pos-vec3( 1.0,0.00,-1.0),vec3(-0.1,0.1,-0.1), vec3(0.2,0.4,0.2), 0.1  ), 31.9 ) );
      res = opU( res, vec2( sdCylinder(     pos-vec3( 1.0,0.25,-2.0), vec2(0.15,0.25) ), 8.0 ) );
      res = opU( res, vec2( sdHexPrism(     pos-vec3( 1.0,0.2,-3.0), vec2(0.2,0.05) ), 18.4 ) );
    }

    if( sdBox( pos-vec3(-1.0,0.35,-1.0),vec3(0.35,0.35,2.5))<res.x ) {
      res = opU( res, vec2( sdPyramid(    pos-vec3(-1.0,-0.6,-3.0), 1.0 ), 13.56 ) );
      res = opU( res, vec2( sdOctahedron( pos-vec3(-1.0,0.15,-2.0), 0.35 ), 23.56 ) );
      res = opU( res, vec2( sdTriPrism(   pos-vec3(-1.0,0.15,-1.0), vec2(0.3,0.05) ),43.5 ) );
      res = opU( res, vec2( sdEllipsoid(  pos-vec3(-1.0,0.25, 0.0), vec3(0.2, 0.25, 0.05) ), 43.17 ) );
    }

    if( sdBox( pos-vec3(2.0,0.3,-1.0),vec3(0.35,0.3,2.5) )<res.x ) {
      res = opU( res, vec2( sdOctogonPrism(pos-vec3( 2.0,0.2,-3.0), 0.2, 0.05), 51.8 ) );
      res = opU( res, vec2( sdCylinder(     pos-vec3( 2.0,0.14,-2.0), vec3(0.1,-0.1,0.0), vec3(-0.2,0.35,0.1), 0.08), 31.2 ) );
      res = opU( res, vec2( sdCappedCone(   pos-vec3( 2.0,0.09,-1.0), vec3(0.1,0.0,0.0), vec3(-0.2,0.40,0.1), 0.15, 0.05), 46.1 ) );
      res = opU( res, vec2( sdRoundCone(    pos-vec3( 2.0,0.15, 0.0), vec3(0.1,0.0,0.0), vec3(-0.1,0.35,0.1), 0.15, 0.05), 51.7 ) );
      res = opU( res, vec2( sdRoundCone(    pos-vec3( 2.0,0.20, 1.0), 0.2, 0.1, 0.3 ), 37.0 ) );
    }
    
    return res;
}

vec2 raycast( vec3 ro, vec3 rd, int iFrame ) {
    vec2 res = vec2(-1.0,-1.0);
    float tmin = 1.0;
    float tmax = 20.0;

    float tp1 = (0.0f-ro.y)/rd.y;
    if( tp1>0.0f ) { tmax = min( tmax, tp1 ); res = vec2( tp1, 1.0 ); }
    
    float t = tmin;
    for( int i=0; i<70 && t<tmax; i++ ) {
        vec2 h = map( ro+rd*t, iFrame );
        if( fabsf(h.x)<(0.0001f*t) ) { res = vec2(t,h.y); break; }
        t += h.x;
    }
    return res;
}

float calcSoftshadow( vec3 ro, vec3 rd, float mint, float tmax, int iFrame ) {
    float tp = (0.8f-ro.y)/rd.y; 
    if( tp>0.0f ) tmax = min( tmax, tp );

    float res = 1.0f;
    float t = mint;
    for( int i=0; i<24; i++ ) {
        float h = map( ro + rd*t, iFrame ).x;
        float s = clamp(8.0f*h/t,0.0f,1.0f);
        res = min( res, s );
        t += clamp( h, 0.01f, 0.2f );
        if( res<0.004f || t>tmax ) break;
    }
    float r = clamp( res, 0.0f, 1.0f );
    return r*r*(3.0f-2.0f*r);
}

vec3 calcNormal( vec3 pos, int iFrame ) {
    vec3 n = vec3(0.0f);
    for( int i=0; i<4; i++ ) {
        vec3 e = 0.5773f*(2.0f*vec3((float)(((i+3)>>1)&1),(float)((i>>1)&1),(float)(i&1))-1.0f);
        n += e*map(pos+0.0005f*e, iFrame).x;
    }
    return normalize(n);
}

float calcAO( vec3 pos, vec3 nor, int iFrame ) {
    float occ = 0.0f;
    float sca = 1.0f;
    for( int i=0; i<5; i++ ) {
        float h = 0.01f + 0.12f*float(i)/4.0f;
        float d = map( pos + h*nor, iFrame ).x;
        occ += (h-d)*sca;
        sca *= 0.95f;
        if( occ>0.35f ) break;
    }
    return clamp( 1.0f - 3.0f*occ, 0.0f, 1.0f ) * (0.5f+0.5f*nor.y);
}

float checkersGradBox( vec2 p, vec2 dpdx, vec2 dpdy ) {
    vec2 w = abs(dpdx)+abs(dpdy) + 0.001f;
    vec2 i = 2.0f*(abs(fract((p-0.5f*w)*0.5f)-0.5f)-abs(fract((p+0.5f*w)*0.5f)-0.5f))/w;
    return 0.5f - 0.5f*i.x*i.y;                  
}

vec3 render( vec3 ro, vec3 rd, vec3 rdx, vec3 rdy, int iFrame ) { 
    vec3 col = vec3(0.7, 0.7, 0.9) - max(rd.y,0.0f)*0.3f;
    vec2 res = raycast(ro,rd, iFrame);
    float t = res.x;
    float m = res.y;
    if( m>-0.5f ) {
        vec3 pos = ro + t*rd;
        vec3 nor = (m<1.5f) ? vec3(0.0f,1.0f,0.0f) : calcNormal( pos, iFrame );
        vec3 ref = reflect( rd, nor );
        
        col = 0.2f + 0.2f*sin( m*2.0f + vec3(0.0,1.0,2.0) );
        float ks = 1.0f;
        
        if( m<1.5f ) {
            vec3 dpdx = ro.y*(rd/rd.y-rdx/rdx.y);
            vec3 dpdy = ro.y*(rd/rd.y-rdy/rdy.y);
            float f = checkersGradBox( 3.0f*pos.xz(), 3.0f*dpdx.xz(), 3.0f*dpdy.xz() );
            col = 0.15f + f*vec3(0.05f);
            ks = 0.4f;
        }

        float occ = calcAO( pos, nor, iFrame );
        vec3 lin = vec3(0.0f);

        // sun
        {
            vec3  lig = normalize( vec3(-0.5, 0.4, -0.6) );
            vec3  hal = normalize( lig-rd );
            float dif = clamp( dot( nor, lig ), 0.0f, 1.0f );
            if( dif>0.0001f ) dif *= calcSoftshadow( pos, lig, 0.02f, 2.5f, iFrame );
            float spe = powf( clamp( dot( nor, hal ), 0.0f, 1.0f ),16.0f);
            spe *= dif;
            spe *= 0.04f+0.96f*powf(clamp(1.0f-dot(hal,lig),0.0f,1.0f),5.0f);
            lin += col*2.20f*dif*vec3(1.30,1.00,0.70);
            lin +=      5.00f*spe*vec3(1.30,1.00,0.70)*ks;
        }
        // sky
        {
            float dif = sqrtf(clamp( 0.5f+0.5f*nor.y, 0.0f, 1.0f ));
            dif *= occ;
            float spe = smoothstep( -0.2f, 0.2f, ref.y );
            spe *= dif;
            spe *= 0.04f+0.96f*powf(clamp(1.0f+dot(nor,rd),0.0f,1.0f), 5.0f );
            if( spe>0.001f ) spe *= calcSoftshadow( pos, ref, 0.02f, 2.5f, iFrame );
            lin += col*0.60f*dif*vec3(0.40,0.60,1.15);
            lin +=      2.00f*spe*vec3(0.40,0.60,1.30)*ks;
        }
        // back
        {
            float dif = clamp( dot( nor, normalize(vec3(0.5,0.0,0.6))), 0.0f, 1.0f )*clamp( 1.0f-pos.y,0.0f,1.0f);
            dif *= occ;
            lin += col*0.55f*dif*vec3(0.25,0.25,0.25);
        }
        // sss
        {
            float dif = powf(clamp(1.0f+dot(nor,rd),0.0f,1.0f),2.0f);
            dif *= occ;
            lin += col*0.25f*dif*vec3(1.00,1.00,1.00);
        }
        col = lin;
        col = mix( col, vec3(0.7,0.7,0.9), 1.0f-expf( -0.0001f*t*t*t ) );
    }
    return clamp(col,0.0f,1.0f);
}

mat3 setCamera( vec3 ro, vec3 ta, float cr ) {
    vec3 cw = normalize(ta-ro);
    vec3 cp = vec3(sinf(cr), cosf(cr),0.0f);
    vec3 cu = normalize( cross(cw,cp) );
    vec3 cv =          ( cross(cu,cw) );
    return mat3( cu, cv, cw );
}

void mainImage( vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime ) {
    vec2 mo = vec2(0.0f); // Default mouse to 0 as Eshi doesn't pass it yet
    float time = 32.0f + iTime*1.5f;
    int iFrame = (int)(iTime * 60.0f);

    vec3 ta = vec3( 0.25, -0.75, -0.75 );
    vec3 ro = ta + vec3( 4.5f*cosf(0.1f*time + 7.0f*mo.x), 2.2f, 4.5f*sinf(0.1f*time + 7.0f*mo.x) );
    mat3 ca = setCamera( ro, ta, 0.0f );

    vec3 tot = vec3(0.0f);
    
    // Simple Anti-Aliasing Loop
    #if AA>1
    for( int m=0; m<AA; m++ )
    for( int n=0; n<AA; n++ ) {
        vec2 o = vec2(float(m),float(n)) / float(AA) - 0.5f;
        vec2 p = (2.0f*(fragCoord+o)-iResolution)/iResolution.y;
    #else    
        vec2 p = (2.0f*fragCoord-iResolution)/iResolution.y;
    #endif

        const float fl = 2.5f;
        vec3 rd = ca * normalize( vec3(p,fl) );

        vec2 px = (2.0f*(fragCoord+vec2(1.0f,0.0f))-iResolution)/iResolution.y;
        vec2 py = (2.0f*(fragCoord+vec2(0.0f,1.0f))-iResolution)/iResolution.y;
        vec3 rdx = ca * normalize( vec3(px,fl) );
        vec3 rdy = ca * normalize( vec3(py,fl) );
        
        vec3 col = render( ro, rd, rdx, rdy, iFrame );
        col = pow( col, vec3(0.4545f) ); // Gamma
        tot += col;
        
    #if AA>1
    }
    tot /= float(AA*AA);
    #endif
    
    fragColor = vec4( tot, 1.0f );
}
