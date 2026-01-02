#include "../glsl_core.h"

using namespace glsl;


#define EPS     0.001f
#define PI      3.14159265359f
#define RADIAN  (180.0f / PI)
#define SPEED   25.0f



SHADER_CTX float hash(float n) {
    return glsl::fract(glsl::sin(n) * 43758.5453123f);
}

SHADER_CTX float hash(vec2 p) {
    return glsl::fract(glsl::sin(glsl::dot(p, vec2(127.1f, 311.7f))) * 43758.5453123f);
}

SHADER_CTX float noise(vec2 p) {
    vec2 i = glsl::floor(p);
    vec2 f = glsl::fract(p);
    
    
    f = f * f * (3.0f - 2.0f * f); 

    vec2 c(0.0f, 1.0f);

    
    
    return glsl::mix(glsl::mix(hash(i + vec2(c.x, c.x)),
                               hash(i + c.yx()), f.x),
                     glsl::mix(hash(i + c.xy()),
                               hash(i + vec2(c.y, c.y)), f.x), f.y);
}

SHADER_CTX float fbm(vec2 p) {
    return  0.5000f * noise(p)
          + 0.2500f * noise(p * 2.0f)
          + 0.1250f * noise(p * 4.0f)
          + 0.0625f * noise(p * 8.0f);
}

SHADER_CTX float dst(vec3 p) {
    
    return glsl::dot(vec3(p.x, p.y
                    + 0.45f * fbm(vec2(p.z, p.x))
                    + 2.55f * noise(0.1f * p.xz())
                    + 0.83f * noise(0.4f * p.xz())
                    + 3.33f * noise(0.001f * p.xz())
                    + 3.59f * noise(0.0005f * (p.xz() + 132.453f))
                    , p.z), vec3(0.0f, 1.0f, 0.0f));
}

SHADER_CTX vec3 nrm(vec3 p, float d) {
    return glsl::normalize(
        vec3(dst(vec3(p.x + EPS, p.y, p.z)),
             dst(vec3(p.x, p.y + EPS, p.z)),
             dst(vec3(p.x, p.y, p.z + EPS))) - d);
}

SHADER_CTX bool rmarch(vec3 ro, vec3 rd, vec3 &p, vec3 &n) {
    p = ro;
    vec3 pos = p;
    float d = 1.0f;

    for (int i = 0; i < 64; i++) {
        d = dst(pos);

        if (d < EPS) {
            p = pos;
            break;
        }
        pos += d * rd;
    }

    n = nrm(p, d);
    return d < EPS;
}


SHADER_CTX vec4 render(vec2 uv, vec2 iResolution, float iTime) {
    float t = iTime;

    vec2 uvn = (uv) * vec2(iResolution.x / iResolution.y, 1.0f);

    float vel = SPEED * t;

    vec3 cu = vec3(2.0f * noise(vec2(0.3f * t)) - 1.0f, 1.0f, 1.0f * fbm(vec2(0.8f * t)));
    vec3 cp = vec3(0.0f, 3.1f + noise(vec2(t)) * 3.1f, vel);

    vec3 ct = vec3(1.5f * glsl::sin(t),
                   -2.0f + glsl::cos(t) + fbm(cp.xz()) * 0.4f, 13.0f + vel);

    vec3 ro = cp;
    
    vec3 rd = glsl::normalize(vec3(uvn.x, uvn.y, 1.0f / glsl::tan(60.0f * RADIAN)));

    vec3 cd = ct - cp;
    vec3 rz = glsl::normalize(cd);
    vec3 rx = glsl::normalize(glsl::cross(rz, cu));
    vec3 ry = glsl::normalize(glsl::cross(rx, rz));

    mat3 rot(rx, ry, rz);
    rd = glsl::normalize(rot * rd);

    vec3 sp, sn;

    vec3 col = (rmarch(ro, rd, sp, sn) ?
        vec3(0.6f) * glsl::dot(sn, glsl::normalize(vec3(cp.x, cp.y + 0.5f, cp.z) - sp))
        : vec3(0.0f));

    return vec4(col.x, col.y, col.z, glsl::length(ro - sp));
}



SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = fragCoord / iResolution.xy() * 2.0f - 1.0f;

    if (glsl::abs(EPS + uv.y) >= 0.7f) {
        fragColor = vec4(0, 0, 0, 1);
        return;
    }

    vec4 res = render(uv, iResolution, iTime);

    vec3 col = res.xyz();

    col *= 1.75f * glsl::smoothstep(glsl::length(uv) * 0.35f, 0.75f, 0.4f);

    float n_val = hash((hash(uv.x) + uv.y) * iTime) * 0.15f;
    col += n_val;
    col *= glsl::smoothstep(EPS, 3.5f, iTime);

    fragColor = vec4(col.x, col.y, col.z, 1.0f);
}
