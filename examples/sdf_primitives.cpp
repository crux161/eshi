#include "../glsl_core.h"
#include <algorithm> 

using namespace glsl;






SHADER_CTX inline float dot2(vec2 v) { return dot(v,v); }
SHADER_CTX inline float dot2(vec3 v) { return dot(v,v); }
SHADER_CTX inline float ndot(vec2 a, vec2 b ) { return a.x*b.x - a.y*b.y; }





#define AA 1 

SHADER_CTX float sdPlane( vec3 p ) {
    return p.y;
}

SHADER_CTX float sdSphere( vec3 p, float s ) {
    return length(p)-s;
}

SHADER_CTX float sdBox( vec3 p, vec3 b ) {
    vec3 d = abs(p) - b;
    return glsl::min(glsl::max(d.x,glsl::max(d.y,d.z)),0.0f) + length(glsl::max(d,0.0f));
}

SHADER_CTX float sdBoxFrame( vec3 p, vec3 b, float e ) {
    p = abs(p)-b;
    vec3 q = abs(p+e)-e;
    return glsl::min(glsl::min(
        length(glsl::max(vec3(p.x,q.y,q.z),0.0f))+glsl::min(glsl::max(p.x,glsl::max(q.y,q.z)),0.0f),
        length(glsl::max(vec3(q.x,p.y,q.z),0.0f))+glsl::min(glsl::max(q.x,glsl::max(p.y,q.z)),0.0f)),
        length(glsl::max(vec3(q.x,q.y,p.z),0.0f))+glsl::min(glsl::max(q.x,glsl::max(q.y,p.z)),0.0f));
}

SHADER_CTX float sdEllipsoid( vec3 p, vec3 r ) {
    float k0 = length(p/r);
    float k1 = length(p/(r*r));
    return k0*(k0-1.0f)/k1;
}

SHADER_CTX float sdTorus( vec3 p, vec2 t ) {
    return length( vec2(length(p.xz())-t.x,p.y) )-t.y;
}

SHADER_CTX float sdCappedTorus( vec3 p, vec2 sc, float ra, float rb) {
    p.x = fabsf(p.x);
    float k = (sc.y*p.x>sc.x*p.y) ? dot(p.xy(),sc) : length(p.xy());
    return sqrtf( dot(p,p) + ra*ra - 2.0f*ra*k ) - rb;
}

SHADER_CTX float sdHexPrism( vec3 p, vec2 h ) {
    const vec3 k = vec3(-0.8660254f, 0.5f, 0.57735f);
    p = abs(p);
    p.x -= 2.0f*glsl::min(dot(p.xy(), k.xy()), 0.0f)*k.x;
    p.y -= 2.0f*glsl::min(dot(p.xy(), k.xy()), 0.0f)*k.y;
    vec2 d = vec2(
       length(p.xy() - vec2(clamp(p.x, -k.z*h.x, k.z*h.x), h.x))*sign(p.y - h.x),
       p.z-h.y );
    return glsl::min(glsl::max(d.x,d.y),0.0f) + length(glsl::max(d,0.0f));
}

SHADER_CTX float sdOctogonPrism( vec3 p, float r, float h ) {
  const vec3 k = vec3(-0.9238795325f, 0.3826834323f, 0.4142135623f); 
  p = abs(p);
  p.x -= 2.0f*glsl::min(dot(vec2(k.x,k.y),p.xy()),0.0f)*k.x;
  p.y -= 2.0f*glsl::min(dot(vec2(k.x,k.y),p.xy()),0.0f)*k.y;
  p.x -= 2.0f*glsl::min(dot(vec2(-k.x,k.y),p.xy()),0.0f)*-k.x;
  p.y -= 2.0f*glsl::min(dot(vec2(-k.x,k.y),p.xy()),0.0f)*k.y;
  p.x -= clamp(p.x, -k.z*r, k.z*r);
  p.y -= r;
  vec2 d = vec2( length(p.xy())*sign(p.y), p.z-h );
  return glsl::min(glsl::max(d.x,d.y),0.0f) + length(glsl::max(d,0.0f));
}

SHADER_CTX float sdCapsule( vec3 p, vec3 a, vec3 b, float r ) {
    vec3 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0f, 1.0f );
    return length( pa - ba*h ) - r;
}

SHADER_CTX float sdRoundCone( vec3 p, float r1, float r2, float h ) {
    vec2 q = vec2( length(p.xz()), p.y );
    float b = (r1-r2)/h;
    float a = sqrtf(1.0f-b*b);
    float k = dot(q,vec2(-b,a));
    if( k < 0.0f ) return length(q) - r1;
    if( k > a*h ) return length(q-vec2(0.0f,h)) - r2;
    return dot(q, vec2(a,b) ) - r1;
}


SHADER_CTX float sdRoundCone(vec3 p, vec3 a, vec3 b, float r1, float r2) {
    vec3  ba = b - a;
    float l  = length(ba);
    float cr = (r1-r2)/l;
    float cz = sqrtf(1.0f-cr*cr);
    vec3  k  = vec3(cr,cz,0.0f); 
    vec3  pa = p - a;
    float paba = dot(pa,ba)/l;
    float paba2 = paba*l;
    float x = length(pa - ba*paba) - r1; 
    
    
    
    float baba = dot(ba,ba);
    float papa = dot(pa,pa);
    
    paba = dot(pa,ba); 
    
    float h = l;
    vec2 q = vec2( length(pa - ba*clamp(paba/baba,0.0f,1.0f)), 0.0f ); 
    
    float b2 = (r1-r2)/h;
    float a2 = sqrtf(1.0f-b2*b2);
    float k2 = dot(pa,ba)/baba;
    float y = paba/h;
    float x2 = length(pa - ba*y);
    float k3 = dot(vec2(x2,y), vec2(-b2,a2));
    if( k3 < 0.0f ) return length(pa) - r1;
    if( k3 > a2*h ) return length(pa - ba) - r2;
    return dot(vec2(x2,y), vec2(a2,b2)) - r1;
}

SHADER_CTX float sdTriPrism( vec3 p, vec2 h ) {
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
    return length(glsl::max(vec2(d1,d2),0.0f)) + glsl::min(glsl::max(d1,d2), 0.0f);
}

SHADER_CTX float sdCylinder( vec3 p, vec2 h ) {
    vec2 d = abs(vec2(length(p.xz()),p.y)) - h;
    return glsl::min(glsl::max(d.x,d.y),0.0f) + length(glsl::max(d,0.0f));
}

SHADER_CTX float sdCylinder(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a;
    vec3 ba = b - a;
    float baba = dot(ba,ba);
    float paba = dot(pa,ba);
    float x = length(pa*baba-ba*paba) - r*baba;
    float y = fabsf(paba-baba*0.5f)-baba*0.5f;
    float x2 = x*x;
    float y2 = y*y*baba;
    float d = (glsl::max(x,y)<0.0f)?-glsl::min(x2,y2):(((x>0.0f)?x2:0.0f)+((y>0.0f)?y2:0.0f));
    return sign(d)*sqrtf(fabsf(d))/baba;
}

SHADER_CTX float sdCone( vec3 p, vec2 c, float h ) {
    vec2 q = h*vec2(c.x,-c.y)/c.y;
    vec2 w = vec2( length(p.xz()), p.y );
    vec2 a = w - q*clamp( dot(w,q)/dot(q,q), 0.0f, 1.0f );
    vec2 b = w - q*vec2( clamp( w.x/q.x, 0.0f, 1.0f ), 1.0f );
    float k = sign( q.y );
    float d = glsl::min(dot( a, a ),dot(b, b));
    float s = glsl::max( k*(w.x*q.y-w.y*q.x),k*(w.y-q.y)  );
    return sqrtf(d)*sign(s);
}

SHADER_CTX float sdCappedCone( vec3 p, float h, float r1, float r2 ) {
    vec2 q = vec2( length(p.xz()), p.y );
    vec2 k1 = vec2(r2,h);
    vec2 k2 = vec2(r2-r1,2.0f*h);
    vec2 ca = vec2(q.x-glsl::min(q.x,(q.y < 0.0f)?r1:r2), fabsf(q.y)-h);
    vec2 cb = q - k1 + k2*clamp( dot(k1-q,k2)/dot2(k2), 0.0f, 1.0f );
    float s = (cb.x < 0.0f && ca.y < 0.0f) ? -1.0f : 1.0f;
    return s*sqrtf( glsl::min(dot2(ca),dot2(cb)) );
}


SHADER_CTX float sdCappedCone( vec3 p, vec3 a, vec3 b, float ra, float rb ) {
    float rba  = rb-ra;
    float baba = dot(b-a,b-a);
    float papa = dot(p-a,p-a);
    float paba = dot(p-a,b-a)/baba;
    float x = sqrtf( papa - paba*paba*baba );
    float cax = glsl::max(0.0f,x-((paba<0.5f)?ra:rb));
    float cay = glsl::abs(paba-0.5f)-0.5f;
    float k = rba*rba + baba;
    float f = clamp( (rba*(x-ra)+paba*baba)/k, 0.0f, 1.0f );
    float cbx = x-ra - f*rba;
    float cby = paba - f;
    float s = (cbx < 0.0f && cay < 0.0f) ? -1.0f : 1.0f;
    return s*sqrtf( glsl::min(cax*cax + cay*cay*baba,
                              cbx*cbx + cby*cby*baba) );
}

SHADER_CTX float sdOctahedron(vec3 p, float s) {
    p = abs(p);
    float m = p.x + p.y + p.z - s;
    vec3 q;
         if( 3.0f*p.x < m ) q = vec3(p.x, p.y, p.z);
    else if( 3.0f*p.y < m ) q = vec3(p.y, p.z, p.x);
    else if( 3.0f*p.z < m ) q = vec3(p.z, p.x, p.y);
    else return m*0.57735027f;
    float k = clamp(0.5f*(q.z-q.y+s),0.0f,s); 
    return length(vec3(q.x,q.y-s+k,q.z-k)); 
}

SHADER_CTX float sdPyramid( vec3 p, float h ) {
    float m2 = h*h + 0.25f;
    p.x = fabsf(p.x); p.z = fabsf(p.z);
    if (p.z > p.x) { float t=p.z; p.z=p.x; p.x=t; } 
    p.x -= 0.5f; p.z -= 0.5f;
    vec3 q = vec3( p.z, h*p.y - 0.5f*p.x, h*p.x + 0.5f*p.y);
    float s = glsl::max(-q.x,0.0f);
    float t = clamp( (q.y-0.5f*p.z)/(m2+0.25f), 0.0f, 1.0f );
    float a = m2*(q.x+s)*(q.x+s) + q.y*q.y;
    float b = m2*(q.x+0.5f*t)*(q.x+0.5f*t) + (q.y-m2*t)*(q.y-m2*t);
    float d2 = glsl::min(q.y,-q.x*m2-q.y*0.5f) > 0.0f ? 0.0f : glsl::min(a,b);
    return sqrtf( (d2+q.z*q.z)/m2 ) * sign(glsl::max(q.z,-p.y));
}

SHADER_CTX float sdRhombus(vec3 p, float la, float lb, float h, float ra) {
    p = abs(p);
    vec2 b = vec2(la,lb);
    float f = clamp( (ndot(b,b-2.0f*p.xz()))/dot(b,b), -1.0f, 1.0f );
    vec2 q = vec2(length(p.xz()-0.5f*b*vec2(1.0f-f,1.0f+f))*sign(p.x*b.y+p.z*b.x-b.x*b.y)-ra, p.y-h);
    return glsl::min(glsl::max(q.x,q.y),0.0f) + length(glsl::max(q,0.0f));
}





SHADER_CTX vec2 opU( vec2 d1, vec2 d2 ) {
    return (d1.x<d2.x) ? d1 : d2;
}

SHADER_CTX vec2 map( vec3 pos, int iFrame ) {
    vec2 res = vec2( pos.y, 0.0f );
    
    if( sdBox( pos-vec3(-2.0,0.3,0.25),vec3(0.3,0.3,1.0) ) < res.x ) {
      res = opU( res, vec2( sdSphere(    pos-vec3(-2.0,0.25, 0.0), 0.25 ), 26.9 ) );
      vec3 t = pos-vec3(-2.0,0.25, 1.0);
      res = opU( res, vec2( sdRhombus(  vec3(t.y, t.z, t.x), 0.15, 0.25, 0.04, 0.08 ),17.0 ) );
    }

    if( sdBox( pos-vec3(0.0,0.3,-1.0),vec3(0.35,0.3,2.5) )<res.x ) {
      res = opU( res, vec2( sdCappedTorus((pos-vec3( 0.0,0.30, 1.0))*vec3(1,-1,1), vec2(0.866025,-0.5), 0.25, 0.05), 25.0) );
      res = opU( res, vec2( sdBoxFrame(    pos-vec3( 0.0,0.25, 0.0), vec3(0.3,0.25,0.2), 0.025 ), 16.9 ) );
      res = opU( res, vec2( sdCone(        pos-vec3( 0.0,0.45,-1.0), vec2(0.6,0.8),0.45 ), 55.0 ) );
      res = opU( res, vec2( sdCappedCone(  pos-vec3( 0.0,0.25,-2.0), 0.1, 0.25, 0.25 ), 13.67 ) );
    }

    if( sdBox( pos-vec3(1.0,0.3,-1.0),vec3(0.35,0.3,2.5) )<res.x ) {
      vec3 t = pos-vec3( 1.0,0.30, 1.0);
      res = opU( res, vec2( sdTorus(       vec3(t.y, t.z, t.x), vec2(0.25,0.05) ), 7.1 ) );
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

SHADER_CTX vec2 raycast( vec3 ro, vec3 rd, int iFrame ) {
    vec2 res = vec2(-1.0,-1.0);
    float tmin = 1.0;
    float tmax = 20.0;

    float tp1 = (0.0f-ro.y)/rd.y;
    if( tp1>0.0f ) { tmax = glsl::min( tmax, tp1 ); res = vec2( tp1, 1.0 ); }
    
    float t = tmin;
    for( int i=0; i<70 && t<tmax; i++ ) {
        vec2 h = map( ro+rd*t, iFrame );
        if( fabsf(h.x)<(0.0001f*t) ) { res = vec2(t,h.y); break; }
        t += h.x;
    }
    return res;
}

SHADER_CTX float calcSoftshadow( vec3 ro, vec3 rd, float mint, float tmax, int iFrame ) {
    float tp = (0.8f-ro.y)/rd.y; 
    if( tp>0.0f ) tmax = glsl::min( tmax, tp );

    float res = 1.0f;
    float t = mint;
    for( int i=0; i<24; i++ ) {
        float h = map( ro + rd*t, iFrame ).x;
        float s = clamp(8.0f*h/t,0.0f,1.0f);
        res = glsl::min( res, s );
        t += clamp( h, 0.01f, 0.2f );
        if( res<0.004f || t>tmax ) break;
    }
    float r = clamp( res, 0.0f, 1.0f );
    return r*r*(3.0f-2.0f*r);
}

SHADER_CTX vec3 calcNormal( vec3 pos, int iFrame ) {
    vec3 n = vec3(0.0f);
    for( int i=0; i<4; i++ ) {
        vec3 e = 0.5773f*(2.0f*vec3((float)(((i+3)>>1)&1),(float)((i>>1)&1),(float)(i&1))-1.0f);
        n += e*map(pos+0.0005f*e, iFrame).x;
    }
    return normalize(n);
}

SHADER_CTX float calcAO( vec3 pos, vec3 nor, int iFrame ) {
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

SHADER_CTX float checkersGradBox( vec2 p, vec2 dpdx, vec2 dpdy ) {
    vec2 w = abs(dpdx)+abs(dpdy) + 0.001f;
    vec2 i = 2.0f*(abs(fract((p-0.5f*w)*0.5f)-0.5f)-abs(fract((p+0.5f*w)*0.5f)-0.5f))/w;
    return 0.5f - 0.5f*i.x*i.y;                  
}

SHADER_CTX vec3 render( vec3 ro, vec3 rd, vec3 rdx, vec3 rdy, int iFrame ) { 
    vec3 col = vec3(0.7, 0.7, 0.9) - glsl::max(rd.y,0.0f)*0.3f;
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
        
        {
            float dif = clamp( dot( nor, normalize(vec3(0.5,0.0,0.6))), 0.0f, 1.0f )*clamp( 1.0f-pos.y,0.0f,1.0f);
            dif *= occ;
            lin += col*0.55f*dif*vec3(0.25,0.25,0.25);
        }
        
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

SHADER_CTX mat3 setCamera( vec3 ro, vec3 ta, float cr ) {
    vec3 cw = normalize(ta-ro);
    vec3 cp = vec3(sinf(cr), cosf(cr),0.0f);
    vec3 cu = normalize( cross(cw,cp) );
    vec3 cv =          ( cross(cu,cw) );
    return mat3( cu, cv, cw );
}

SHADER_CTX void mainImage( vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime ) {
    vec2 mo = vec2(0.0f); 
    float time = 32.0f + iTime*1.5f;
    int iFrame = (int)(iTime * 60.0f);

    vec3 ta = vec3( 0.25, -0.75, -0.75 );
    vec3 ro = ta + vec3( 4.5f*cosf(0.1f*time + 7.0f*mo.x), 2.2f, 4.5f*sinf(0.1f*time + 7.0f*mo.x) );
    mat3 ca = setCamera( ro, ta, 0.0f );

    vec3 tot = vec3(0.0f);
    
    vec2 p = (2.0f*fragCoord-iResolution)/iResolution.y;

    const float fl = 2.5f;
    vec3 rd = ca * normalize( vec3(p,fl) );

    vec2 px = (2.0f*(fragCoord+vec2(1.0f,0.0f))-iResolution)/iResolution.y;
    vec2 py = (2.0f*(fragCoord+vec2(0.0f,1.0f))-iResolution)/iResolution.y;
    vec3 rdx = ca * normalize( vec3(px,fl) );
    vec3 rdy = ca * normalize( vec3(py,fl) );
    
    vec3 col = render( ro, rd, rdx, rdy, iFrame );
    col = pow( col, vec3(0.4545f) ); 
    tot += col;
    
    fragColor = vec4( tot, 1.0f );
}
