#include "../glsl_core.h"

using namespace glsl;

#define LOWQUALITY
#define ZERO 0

SHADER_CTX vec3 yzw(const vec4& v) { return vec3(v.y, v.z, v.w); }
SHADER_CTX vec2 yz(const vec3& v) { return vec2(v.y, v.z); }

SHADER_CTX float sdEllipsoidY(vec3 p, vec2 r) {
    vec3 rr = vec3(r.x, r.y, r.x);
    float k0 = length(p / rr);
    float k1 = length(p / (rr * rr));
    return k0 * (k0 - 1.0f) / k1;
}

SHADER_CTX vec2 smoothstepd(float a, float b, float x) {
    if (x < a) return vec2(0.0f, 0.0f);
    if (x > b) return vec2(1.0f, 0.0f);
    float ir = 1.0f / (b - a);
    x = (x - a) * ir;
    return vec2(x * x * (3.0f - 2.0f * x), 6.0f * x * (1.0f - x) * ir);
}

SHADER_CTX mat3 setCamera(vec3 ro, vec3 ta, float cr) {
    vec3 cw = normalize(ta - ro);
    vec3 cp = vec3(glsl::sin(cr), glsl::cos(cr), 0.0f);
    vec3 cu = normalize(cross(cw, cp));
    vec3 cv = normalize(cross(cu, cw));
    return mat3(cu, cv, cw);
}

SHADER_CTX float hash1(vec2 p) {
    p = 50.0f * fract(p * 0.3183099f);
    return fract(p.x * p.y * (p.x + p.y));
}

SHADER_CTX float hash1(float n) {
    return fract(n * 17.0f * fract(n * 0.3183099f));
}

SHADER_CTX vec2 hash2(vec2 p) {
    const vec2 k = vec2(0.3183099f, 0.3678794f);
    float n = 111.0f * p.x + 113.0f * p.y;
    return fract(n * fract(k * n));
}

SHADER_CTX vec4 noised(vec3 x) {
    vec3 p = floor(x);
    vec3 w = fract(x);
    vec3 u = w * w * w * (w * (w * 6.0f - 15.0f) + 10.0f);
    vec3 du = 30.0f * w * w * (w * (w - 2.0f) + 1.0f);

    float n = p.x + 317.0f * p.y + 157.0f * p.z;

    float a = hash1(n + 0.0f);
    float b = hash1(n + 1.0f);
    float c = hash1(n + 317.0f);
    float d = hash1(n + 318.0f);
    float e = hash1(n + 157.0f);
    float f = hash1(n + 158.0f);
    float g = hash1(n + 474.0f);
    float h = hash1(n + 475.0f);

    float k0 = a;
    float k1 = b - a;
    float k2 = c - a;
    float k3 = e - a;
    float k4 = a - b - c + d;
    float k5 = a - c - e + g;
    float k6 = a - b - e + f;
    float k7 = -a + b + c - d + e - f - g + h;

    vec3 der = 2.0f * du * vec3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z,
                                k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x,
                                k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);

    return vec4(-1.0f + 2.0f * (k0 + k1 * u.x + k2 * u.y + k3 * u.z +
                                k4 * u.x * u.y + k5 * u.y * u.z +
                                k6 * u.z * u.x + k7 * u.x * u.y * u.z), der);
}

SHADER_CTX float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 w = fract(x);
    vec3 u = w * w * w * (w * (w * 6.0f - 15.0f) + 10.0f);

    float n = p.x + 317.0f * p.y + 157.0f * p.z;

    float a = hash1(n + 0.0f);
    float b = hash1(n + 1.0f);
    float c = hash1(n + 317.0f);
    float d = hash1(n + 318.0f);
    float e = hash1(n + 157.0f);
    float f = hash1(n + 158.0f);
    float g = hash1(n + 474.0f);
    float h = hash1(n + 475.0f);

    float k0 = a;
    float k1 = b - a;
    float k2 = c - a;
    float k3 = e - a;
    float k4 = a - b - c + d;
    float k5 = a - c - e + g;
    float k6 = a - b - e + f;
    float k7 = -a + b + c - d + e - f - g + h;

    return -1.0f + 2.0f * (k0 + k1 * u.x + k2 * u.y + k3 * u.z +
                           k4 * u.x * u.y + k5 * u.y * u.z +
                           k6 * u.z * u.x + k7 * u.x * u.y * u.z);
}

SHADER_CTX vec3 noised(vec2 x) {
    vec2 p = floor(x);
    vec2 w = fract(x);
    vec2 u = w * w * w * (w * (w * 6.0f - 15.0f) + 10.0f);
    vec2 du = 30.0f * w * w * (w * (w - 2.0f) + 1.0f);

    float a = hash1(p + vec2(0.0f, 0.0f));
    float b = hash1(p + vec2(1.0f, 0.0f));
    float c = hash1(p + vec2(0.0f, 1.0f));
    float d = hash1(p + vec2(1.0f, 1.0f));

    float k0 = a;
    float k1 = b - a;
    float k2 = c - a;
    float k4 = a - b - c + d;

    return vec3(-1.0f + 2.0f * (k0 + k1 * u.x + k2 * u.y + k4 * u.x * u.y),
                2.0f * du * vec2(k1 + k4 * u.y, k2 + k4 * u.x));
}

SHADER_CTX float noise(vec2 x) {
    vec2 p = floor(x);
    vec2 w = fract(x);
    vec2 u = w * w * w * (w * (w * 6.0f - 15.0f) + 10.0f);

    float a = hash1(p + vec2(0.0f, 0.0f));
    float b = hash1(p + vec2(1.0f, 0.0f));
    float c = hash1(p + vec2(0.0f, 1.0f));
    float d = hash1(p + vec2(1.0f, 1.0f));

    return -1.0f + 2.0f * (a + (b - a) * u.x + (c - a) * u.y +
                           (a - b - c + d) * u.x * u.y);
}

const mat3 m3 = mat3(0.00f, 0.80f, 0.60f,
                    -0.80f, 0.36f, -0.48f,
                    -0.60f, -0.48f, 0.64f);
const mat3 m3i = mat3(0.00f, -0.80f, -0.60f,
                      0.80f, 0.36f, -0.48f,
                      0.60f, -0.48f, 0.64f);
const mat2 m2 = mat2(0.80f, 0.60f,
                    -0.60f, 0.80f);
const mat2 m2i = mat2(0.80f, -0.60f,
                      0.60f, 0.80f);

SHADER_CTX float fbm_4(vec2 x) {
    float f = 1.9f;
    float s = 0.55f;
    float a = 0.0f;
    float b = 0.5f;
    for (int i = ZERO; i < 4; i++) {
        float n = noise(x);
        a += b * n;
        b *= s;
        x = (f * m2) * x;
    }
    return a;
}

SHADER_CTX float fbm_4(vec3 x) {
    float f = 2.0f;
    float s = 0.5f;
    float a = 0.0f;
    float b = 0.5f;
    for (int i = ZERO; i < 4; i++) {
        float n = noise(x);
        a += b * n;
        b *= s;
        x = (f * m3) * x;
    }
    return a;
}

SHADER_CTX vec4 fbmd_7(vec3 x) {
    float f = 1.92f;
    float s = 0.5f;
    float a = 0.0f;
    float b = 0.5f;
    vec3 d = vec3(0.0f);
    mat3 m = mat3(1.0f);
    for (int i = ZERO; i < 7; i++) {
        vec4 n = noised(x);
        a += b * n.x;
        d += b * (m * yzw(n));
        b *= s;
        x = (f * m3) * x;
        m = (f * m3i) * m;
    }
    return vec4(a, d);
}

SHADER_CTX vec4 fbmd_8(vec3 x) {
    float f = 2.0f;
    float s = 0.65f;
    float a = 0.0f;
    float b = 0.5f;
    vec3 d = vec3(0.0f);
    mat3 m = mat3(1.0f);
    for (int i = ZERO; i < 8; i++) {
        vec4 n = noised(x);
        a += b * n.x;
        if (i < 4) d += b * (m * yzw(n));
        b *= s;
        x = (f * m3) * x;
        m = (f * m3i) * m;
    }
    return vec4(a, d);
}

SHADER_CTX float fbm_9(vec2 x) {
    float f = 1.9f;
    float s = 0.55f;
    float a = 0.0f;
    float b = 0.5f;
    for (int i = ZERO; i < 9; i++) {
        float n = noise(x);
        a += b * n;
        b *= s;
        x = (f * m2) * x;
    }
    return a;
}

SHADER_CTX vec3 fbmd_9(vec2 x) {
    float f = 1.9f;
    float s = 0.55f;
    float a = 0.0f;
    float b = 0.5f;
    vec2 d = vec2(0.0f);
    mat2 m = mat2(1.0f);
    for (int i = ZERO; i < 9; i++) {
        vec3 n = noised(x);
        a += b * n.x;
        d += b * (m * yz(n));
        b *= s;
        x = (f * m2) * x;
        m = (f * m2i) * m;
    }
    return vec3(a, d);
}

const vec3 kSunDir = vec3(-0.624695f, 0.468521f, -0.624695f);
const float kMaxTreeHeight = 4.8f;
const float kMaxHeight = 840.0f;

SHADER_CTX vec3 fog(vec3 col, float t) {
    vec3 ext = exp2(-t * 0.00025f * vec3(1.0f, 1.5f, 4.0f));
    return col * ext + (1.0f - ext) * vec3(0.55f, 0.55f, 0.58f);
}

SHADER_CTX vec4 cloudsFbm(vec3 pos, float iTime) {
    return fbmd_8(pos * 0.0015f + vec3(2.0f, 1.1f, 1.0f) +
                  0.07f * vec3(iTime, 0.5f * iTime, -0.15f * iTime));
}

SHADER_CTX vec4 cloudsMap(vec3 pos, float iTime, float& nnd) {
    float d = glsl::abs(pos.y - 900.0f) - 40.0f;
    vec3 gra = vec3(0.0f, sign(pos.y - 900.0f), 0.0f);

    vec4 n = cloudsFbm(pos, iTime);
    d += 400.0f * n.x * (0.7f + 0.3f * gra.y);

    if (d > 0.0f) return vec4(-d, 0.0f, 0.0f, 0.0f);

    nnd = -d;
    d = min(-d / 100.0f, 0.25f);
    return vec4(d, gra);
}

SHADER_CTX float cloudsShadowFlat(vec3 ro, vec3 rd, float iTime) {
    float t = (900.0f - ro.y) / rd.y;
    if (t < 0.0f) return 1.0f;
    vec3 pos = ro + rd * t;
    return cloudsFbm(pos, iTime).x;
}

SHADER_CTX vec2 terrainMap(vec2 p);
SHADER_CTX float terrainShadow(vec3 ro, vec3 rd, float mint);

SHADER_CTX vec4 renderClouds(vec3 ro, vec3 rd, float tmin, float tmax,
                             float& resT, vec2 px, float iTime) {
    (void)px;
    vec4 sum = vec4(0.0f);

    float tl = (600.0f - ro.y) / rd.y;
    float th = (1200.0f - ro.y) / rd.y;
    if (tl > 0.0f) tmin = max(tmin, tl); else return sum;
    if (th > 0.0f) tmax = min(tmax, th);

    float t = tmin;
    float lastT = -1.0f;
    float thickness = 0.0f;
    for (int i = ZERO; i < 128; i++) {
        vec3 pos = ro + t * rd;
        float nnd;
        vec4 denGra = cloudsMap(pos, iTime, nnd);
        float den = denGra.x;
        float dt = max(0.2f, 0.011f * t);

        if (den > 0.001f) {
            float kk;
            cloudsMap(pos + kSunDir * 70.0f, iTime, kk);
            float sha = 1.0f - smoothstep(-200.0f, 200.0f, kk);
            sha *= 1.5f;

            vec3 nor = normalize(yzw(denGra));
            float dif = clamp(0.4f + 0.6f * dot(nor, kSunDir), 0.0f, 1.0f) * sha;
            float occ = 0.2f + 0.7f * max(1.0f - kk / 200.0f, 0.0f) + 0.1f * (1.0f - den);

            vec3 lin = vec3(0.0f);
            lin += vec3(0.70f, 0.80f, 1.00f) * (0.5f + 0.5f * nor.y) * occ;
            lin += vec3(0.10f, 0.40f, 0.20f) * (0.5f - 0.5f * nor.y) * occ;
            lin += vec3(1.00f, 0.95f, 0.85f) * 3.0f * dif * occ + 0.1f;

            vec3 col = vec3(0.8f) * 0.45f * lin;
            col = fog(col, t);

            float alp = clamp(den * 0.5f * 0.125f * dt, 0.0f, 1.0f);
            col *= alp;
            sum = sum + vec4(col, alp) * (1.0f - sum.w);

            thickness += dt * den;
            if (lastT < 0.0f) lastT = t;
        } else {
            dt = glsl::abs(den) + 0.2f;
        }

        t += dt;
        if (sum.w > 0.995f || t > tmax) break;
    }

    if (lastT > 0.0f) resT = min(resT, lastT);

    vec3 glow = max(0.0f, 1.0f - 0.0125f * thickness) *
                vec3(1.00f, 0.60f, 0.40f) * 0.3f *
                glsl::pow(clamp(dot(kSunDir, rd), 0.0f, 1.0f), 32.0f);
    sum.x += glow.x;
    sum.y += glow.y;
    sum.z += glow.z;

    return clamp(sum, 0.0f, 1.0f);
}

SHADER_CTX vec2 terrainMap(vec2 p) {
    float e = fbm_9(p / 2000.0f + vec2(1.0f, -2.0f));
    float a = 1.0f - smoothstep(0.12f, 0.13f, glsl::abs(e + 0.12f));
    e = 600.0f * e + 600.0f;
    e += 90.0f * smoothstep(552.0f, 594.0f, e);
    return vec2(e, a);
}

SHADER_CTX vec4 terrainMapD(vec2 p) {
    vec3 e = fbmd_9(p / 2000.0f + vec2(1.0f, -2.0f));
    e.x = 600.0f * e.x + 600.0f;
    e.y *= 600.0f;
    e.z *= 600.0f;

    vec2 c = smoothstepd(550.0f, 600.0f, e.x);
    e.x += 90.0f * c.x;
    e.y += 90.0f * c.y * e.y;
    e.z += 90.0f * c.y * e.z;

    e.y /= 2000.0f;
    e.z /= 2000.0f;
    return vec4(e.x, normalize(vec3(-e.y, 1.0f, -e.z)));
}

SHADER_CTX vec3 terrainNormal(vec2 pos) {
    return yzw(terrainMapD(pos));
}

SHADER_CTX float terrainShadow(vec3 ro, vec3 rd, float mint) {
    float res = 1.0f;
    float t = mint;
#ifdef LOWQUALITY
    for (int i = ZERO; i < 32; i++) {
#else
    for (int i = ZERO; i < 128; i++) {
#endif
        vec3 pos = ro + t * rd;
        vec2 env = terrainMap(pos.xz());
        float hei = pos.y - env.x;
        res = min(res, 32.0f * hei / t);
        if (res < 0.0001f || pos.y > kMaxHeight) break;
#ifdef LOWQUALITY
        t += clamp(hei, 2.0f + t * 0.1f, 100.0f);
#else
        t += clamp(hei, 0.5f + t * 0.05f, 25.0f);
#endif
    }
    return clamp(res, 0.0f, 1.0f);
}

SHADER_CTX vec2 raymarchTerrain(vec3 ro, vec3 rd, float tmin, float tmax) {
    float tp = (kMaxHeight + kMaxTreeHeight - ro.y) / rd.y;
    if (tp > 0.0f) tmax = min(tmax, tp);

    float dis = 0.0f;
    float th = 0.0f;
    float t2 = -1.0f;
    float t = tmin;
    float ot = t;
    float odis = 0.0f;
    float odis2 = 0.0f;
    for (int i = ZERO; i < 400; i++) {
        th = 0.001f * t;

        vec3 pos = ro + t * rd;
        vec2 env = terrainMap(pos.xz());
        float hei = env.x;

        float dis2 = pos.y - (hei + kMaxTreeHeight * 1.1f);
        if (dis2 < th && t2 < 0.0f) {
            t2 = ot + (th - odis2) * (t - ot) / (dis2 - odis2);
        }
        odis2 = dis2;

        dis = pos.y - hei;
        if (dis < th) break;

        ot = t;
        odis = dis;
        t += dis * 0.8f * (1.0f - 0.75f * env.y);
        if (t > tmax) break;
    }

    if (t > tmax) {
        t = -1.0f;
    } else {
        t = ot + (th - odis) * (t - ot) / (dis - odis);
    }

    return vec2(t, t2);
}

SHADER_CTX float treesMap(vec3 p, float rt, float& oHei, float& oMat, float& oDis) {
    oHei = 1.0f;
    oDis = 0.0f;
    oMat = 0.0f;

    float base = terrainMap(p.xz()).x;
    float bb = fbm_4(p.xz() * 0.075f);

    float d = 20.0f;
    vec2 n = floor(p.xz() / 2.0f);
    vec2 f = fract(p.xz() / 2.0f);
    for (int j = 0; j <= 1; j++) {
        for (int i = 0; i <= 1; i++) {
            vec2 g = vec2((float)i, (float)j) - step(f, vec2(0.5f));
            vec2 o = hash2(n + g);
            vec2 v = hash2(n + g + vec2(13.1f, 71.7f));
            vec2 r = g - f + o;

            float height = kMaxTreeHeight * (0.4f + 0.8f * v.x);
            float width = 0.5f + 0.2f * v.x + 0.3f * v.y;

            if (bb < 0.0f) width *= 0.5f; else height *= 0.7f;

            vec3 q = vec3(r.x, p.y - base - height * 0.5f, r.y);
            float k = sdEllipsoidY(q, vec2(width, 0.5f * height));

            if (k < d) {
                d = k;
                oMat = 0.5f * hash1(n + g + 111.0f);
                if (bb > 0.0f) oMat += 0.5f;
                oHei = (p.y - base) / height;
                oHei *= 0.5f + 0.5f * length(q) / width;
            }
        }
    }

    if (rt < 1200.0f) {
        p.y -= 600.0f;
        float s = fbm_4(p * 3.0f);
        s = s * s;
        float att = 1.0f - smoothstep(100.0f, 1200.0f, rt);
        d += 4.0f * s * att;
        oDis = s * att;
    }

    return d;
}

SHADER_CTX float treesShadow(vec3 ro, vec3 rd) {
    float res = 1.0f;
    float t = 0.02f;
#ifdef LOWQUALITY
    for (int i = ZERO; i < 64; i++) {
#else
    for (int i = ZERO; i < 150; i++) {
#endif
        float kk1, kk2, kk3;
        vec3 pos = ro + rd * t;
        float h = treesMap(pos, t, kk1, kk2, kk3);
        res = min(res, 32.0f * h / t);
        t += h;
#ifdef LOWQUALITY
        if (res < 0.001f || t > 50.0f || pos.y > kMaxHeight + kMaxTreeHeight) break;
#else
        if (res < 0.001f || t > 120.0f) break;
#endif
    }
    return clamp(res, 0.0f, 1.0f);
}

SHADER_CTX vec3 treesNormal(vec3 pos, float t) {
    float kk1, kk2, kk3;
    vec3 n = vec3(0.0f);
    for (int i = ZERO; i < 4; i++) {
        vec3 e = 0.5773f * (2.0f * vec3((float)(((i + 3) >> 1) & 1),
                                          (float)((i >> 1) & 1),
                                          (float)(i & 1)) - 1.0f);
        n += e * treesMap(pos + 0.005f * e, t, kk1, kk2, kk3);
    }
    return normalize(n);
}

SHADER_CTX vec3 renderSky(vec3 ro, vec3 rd) {
    vec3 col = vec3(0.42f, 0.62f, 1.1f) - rd.y * 0.4f;

    float t = (2500.0f - ro.y) / rd.y;
    if (t > 0.0f) {
        vec2 uv = (ro + t * rd).xz();
        float cl = fbm_9(uv * 0.00104f);
        float dl = smoothstep(-0.2f, 0.6f, cl);
        col = mix(col, vec3(1.0f), 0.12f * dl);
    }

    float sun = clamp(dot(kSunDir, rd), 0.0f, 1.0f);
    col += 0.2f * vec3(1.0f, 0.6f, 0.3f) * glsl::pow(sun, 32.0f);

    return col;
}

SHADER_CTX vec3 renderRainforest(vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 p = (2.0f * fragCoord - iResolution) / iResolution.y;

    float time = iTime;
    vec3 ro = vec3(0.0f, 401.5f, 6.0f);
    vec3 ta = vec3(0.0f, 403.5f, -90.0f + ro.z);

    ro.x -= 80.0f * glsl::sin(0.01f * time);
    ta.x -= 86.0f * glsl::sin(0.01f * time);

    mat3 ca = setCamera(ro, ta, 0.0f);
    vec3 rd = ca * normalize(vec3(p, 1.5f));

    float resT = 2000.0f;
    vec3 col = renderSky(ro, rd);

    const float tmax = 2000.0f;
    int obj = 0;
    vec2 t = raymarchTerrain(ro, rd, 15.0f, tmax);
    if (t.x > 0.0f) {
        resT = t.x;
        obj = 1;
    }

    float hei = 1.0f;
    float mid = 0.0f;
    float displa = 0.0f;

    if (t.y > 0.0f) {
        float tf = t.y;
        float tfMax = (t.x > 0.0f) ? t.x : tmax;
        for (int i = ZERO; i < 64; i++) {
            vec3 pos = ro + tf * rd;
            float dis = treesMap(pos, tf, hei, mid, displa);
            if (dis < (0.000125f * tf)) break;
            tf += dis;
            if (tf > tfMax) break;
        }
        if (tf < tfMax) {
            resT = tf;
            obj = 2;
        }
    }

    if (obj > 0) {
        vec3 pos = ro + resT * rd;
        vec3 epos = pos + vec3(0.0f, 4.8f, 0.0f);

        float sha1 = terrainShadow(pos + vec3(0.0f, 0.02f, 0.0f), kSunDir, 0.02f);
        sha1 *= smoothstep(-0.325f, -0.075f, cloudsShadowFlat(epos, kSunDir, iTime));

#ifndef LOWQUALITY
        float sha2 = treesShadow(pos + vec3(0.0f, 0.02f, 0.0f), kSunDir);
#endif

        vec3 tnor = terrainNormal(pos.xz());
        vec3 nor;
        vec3 speC = vec3(1.0f);

        if (obj == 1) {
            vec3 bump = yzw(fbmd_7((pos - vec3(0.0f, 600.0f, 0.0f)) *
                                   0.15f * vec3(1.0f, 0.2f, 1.0f)));
            nor = normalize(tnor + 0.64f * (1.0f - glsl::abs(tnor.y)) * bump);

            col = vec3(0.18f, 0.12f, 0.10f) * 0.85f;
            col = mix(col, vec3(0.1f, 0.1f, 0.0f) * 0.2f, smoothstep(0.7f, 0.9f, nor.y));
            float dif = clamp(dot(nor, kSunDir), 0.0f, 1.0f) * sha1;
#ifndef LOWQUALITY
            dif *= sha2;
#endif
            float bac = clamp(dot(normalize(vec3(-kSunDir.x, 0.0f, -kSunDir.z)), nor), 0.0f, 1.0f);
            float foc = clamp((pos.y / 2.0f - 180.0f) / 130.0f, 0.0f, 1.0f);
            float dom = clamp(0.5f + 0.5f * nor.y, 0.0f, 1.0f);
            vec3 lin = 0.2f * mix(0.1f * vec3(0.1f, 0.2f, 0.1f), vec3(0.7f, 0.9f, 1.5f) * 3.0f, dom) * foc;
            lin += 8.5f * vec3(1.0f, 0.9f, 0.8f) * dif;
            lin += 0.27f * vec3(1.1f, 1.0f, 0.9f) * bac * foc;
            speC = vec3(4.0f) * dif * smoothstep(20.0f, 0.0f, glsl::abs(pos.y / 2.0f - 310.0f) - 20.0f);

            col *= lin;
        } else {
            vec3 gnor = treesNormal(pos, resT);
            nor = normalize(gnor + 2.0f * tnor);

            float occ = clamp(hei, 0.0f, 1.0f) * glsl::pow(1.0f - 2.0f * displa, 3.0f);
            float dif = clamp(0.1f + 0.9f * dot(nor, kSunDir), 0.0f, 1.0f) * sha1;
            if (dif > 0.0001f) {
                float a = clamp(0.5f + 0.5f * dot(tnor, kSunDir), 0.0f, 1.0f);
                a = a * a;
                a *= occ;
                a *= 0.6f;
                a *= smoothstep(60.0f, 200.0f, resT);
#ifdef LOWQUALITY
                float sha2 = treesShadow(pos + kSunDir * 0.1f, kSunDir);
#endif
                dif *= a + (1.0f - a) * sha2;
            }
            float dom = clamp(0.5f + 0.5f * nor.y, 0.0f, 1.0f);
            float bac = clamp(0.5f + 0.5f * dot(normalize(vec3(-kSunDir.x, 0.0f, -kSunDir.z)), nor), 0.0f, 1.0f);
            float fre = clamp(1.0f + dot(nor, rd), 0.0f, 1.0f);

            vec3 lin = 12.0f * vec3(1.2f, 1.0f, 0.7f) * dif * occ *
                       (2.5f - 1.5f * smoothstep(0.0f, 120.0f, resT));
            lin += 0.55f * mix(0.1f * vec3(0.1f, 0.2f, 0.0f), vec3(0.6f, 1.0f, 1.0f), dom * occ);
            lin += 0.07f * vec3(1.0f, 1.0f, 0.9f) * bac * occ;
            lin += 1.10f * vec3(0.9f, 1.0f, 0.8f) * glsl::pow(fre, 5.0f) * occ *
                   (1.0f - smoothstep(100.0f, 200.0f, resT));
            speC = dif * vec3(1.0f, 1.1f, 1.5f) * 1.2f;

            float brownAreas = fbm_4(pos.zx() * 0.015f);
            col = vec3(0.2f, 0.2f, 0.05f);
            col = mix(col, vec3(0.32f, 0.2f, 0.05f), smoothstep(0.2f, 0.9f, fract(2.0f * mid)));
            float midFade = 0.65f + 0.35f * smoothstep(300.0f, 600.0f, resT) * smoothstep(700.0f, 500.0f, pos.y);
            col *= (mid < 0.5f) ? midFade : 1.0f;
            col = mix(col, vec3(0.25f, 0.16f, 0.01f) * 0.825f,
                      0.7f * smoothstep(0.1f, 0.3f, brownAreas) * smoothstep(0.5f, 0.8f, tnor.y));
            col *= 1.0f - 0.5f * smoothstep(400.0f, 700.0f, pos.y);
            col *= lin;
        }

        vec3 ref = reflect(rd, nor);
        float fre = clamp(1.0f + dot(nor, rd), 0.0f, 1.0f);
        float spe = 3.0f * glsl::pow(clamp(dot(ref, kSunDir), 0.0f, 1.0f), 9.0f) *
                    (0.05f + 0.95f * glsl::pow(fre, 5.0f));
        col += spe * speC;

        col = fog(col, resT);
    }

    float isCloud = 0.0f;
    vec4 res = renderClouds(ro, rd, 0.0f, resT, resT, fragCoord, iTime);
    col = col * (1.0f - res.w) + res.xyz();
    isCloud = res.w;

    float sun = clamp(dot(kSunDir, rd), 0.0f, 1.0f);
    col += 0.25f * vec3(0.8f, 0.4f, 0.2f) * glsl::pow(sun, 4.0f);

    col = glsl::pow(clamp(col * 1.1f - 0.02f, 0.0f, 1.0f), vec3(0.4545f));
    col = col * col * (3.0f - 2.0f * col);
    col = glsl::pow(col, vec3(1.0f, 0.92f, 1.0f));
    col *= vec3(1.02f, 0.99f, 0.9f);
    col.z += 0.1f;

    col = mix(col, col * vec3(0.94f, 1.02f, 0.98f), isCloud);
    return clamp(col, 0.0f, 1.0f);
}

SHADER_CTX void mainImage(vec4& fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec3 col = renderRainforest(fragCoord, iResolution, iTime);
    fragColor = vec4(col, 1.0f);
}
