#include "../glsl_core.h"

using namespace glsl;

/*
 * Larimar — the light web on a sand bar.
 *
 * Stand in clear water on a bright day and the floor is covered in a moving net
 * of light. The net is not a pattern printed on the sand: it is the place where
 * the sunlight that crossed a wide patch of wavy surface has been squeezed onto
 * a narrow patch of floor. Slope steers the light; curvature is what does the
 * squeezing, and that is the whole of what this shader computes.
 *
 * A surface height field made of six sinusoids displaces every ray by
 * depth * grad(h). Brightness on the floor is the reciprocal of how much that
 * map compresses area, and where the compression passes through zero the sheet
 * of light has folded onto a line. Those lines are the filaments of the web.
 *
 * The colour is the other half of the name. Larimar is a pectolite pulled out
 * of one hillside in the Dominican Republic, turquoise cells outlined in white.
 * Water makes the same picture out of physics rather than mineralogy: the
 * caustic draws the white cell walls, and Beer-Lambert absorption through the
 * column eats the red and leaves the blue between them. The gem's broad bands
 * of paler and deeper blue are the sea floor here — a bar, and the runnels
 * cutting across it.
 */

/* --- the surface overhead ------------------------------------------------ */

#define LC_WAVES 6
/* Where the fan of wave directions starts; the swell comes from over there. */
#define LC_BEARING 0.62f
/* Five sixths of a half turn, which spreads six wave vectors evenly over the
 * half-circle. Not over the circle: a grating and its opposite are the same
 * grating, so a heading only means anything modulo pi, and the golden angle
 * that spreads beautifully over 2*pi puts five of these six within a radian of
 * one axis once you reduce it. The web that comes out is combed the same way,
 * and it looks it. Regularity costs nothing here, because the six waves differ
 * in wavelength, speed, and phase; direction is the one axis they can afford to
 * share out evenly. */
#define LC_TURN 2.61799f
#define LC_FREQUENCY 1.85f
#define LC_LACUNARITY 1.83f
#define LC_AMPLITUDE 0.16f
/* Set against the lacunarity so that amplitude * k^2 stays near flat from one
 * octave to the next. Height is then dominated by the longest wave and
 * curvature is shared out evenly, which is the right way round: curvature is
 * what the web is made of. */
#define LC_GAIN 0.31f
#define LC_CELERITY 1.15f
/* A phase offset per wave. Without it every one of them is a zero crossing at
 * the origin, and frame one opens on a knot of coherence that no later frame
 * has. */
#define LC_STAGGER 2.35f

/* --- the floor underneath ------------------------------------------------ */

/* Camera. LC_PITCH is measured from horizontal, so 1.5708 is straight down. */
#define LC_PITCH 1.16f
#define LC_LENS 1.45f
#define LC_EYE 2.35f
#define LC_DRIFT 0.09f

#define LC_BAR_LINE 1.15f
#define LC_DEPTH_MIN 1.40f
#define LC_DEPTH_SPAN 1.60f
#define LC_TIDE 0.13f
#define LC_RUNNEL 0.36f
#define LC_RIPPLE 24.0f

/* --- light --------------------------------------------------------------- */

#define LC_FOLD_SOFTEN 0.10f
#define LC_WEB_GAIN 0.40f
/* Blue refracts harder than red, so its filament lands a little outside the red
 * one. Small on purpose: this is the colour that comes up along the edges of
 * the net at noon, not a prism. */
#define LC_DISPERSE_R 0.972f
#define LC_DISPERSE_B 1.034f
/* Down through the column and back out to the eye, at a slant. */
#define LC_PATH 1.8f
#define LC_AMBIENT 0.06f
#define LC_HAZE 0.32f
#define LC_SCATTER 0.85f
#define LC_EXPOSURE 1.55f
#define LC_SATURATION 1.30f

/* Absorption per unit depth. Red goes first, which is the whole reason clear
 * water is the colour it is. */
#define LC_ABSORB vec3(0.95f, 0.20f, 0.085f)
#define LC_SHALLOW vec3(0.34f, 0.67f, 0.71f)
#define LC_LAGOON vec3(0.08f, 0.58f, 0.66f)
#define LC_DEEP vec3(0.015f, 0.16f, 0.30f)
#define LC_SAND vec3(0.70f, 0.75f, 0.73f)

SHADER_CTX float lcHash(vec2 p) {
    return glsl::fract(glsl::sin(glsl::dot(p, vec2(127.1f, 311.7f))) * 43758.5453f);
}

SHADER_CTX float lcNoise(vec2 p) {
    vec2 i = glsl::floor(p);
    vec2 f = glsl::fract(p);
    vec2 u = f * f * (3.0f - 2.0f * f);

    float a = lcHash(i);
    float b = lcHash(i + vec2(1.0f, 0.0f));
    float c = lcHash(i + vec2(0.0f, 1.0f));
    float d = lcHash(i + vec2(1.0f, 1.0f));

    return glsl::mix(glsl::mix(a, b, u.x), glsl::mix(c, d, u.x), u.y);
}

/*
 * Curvature of the water surface at p, packed as (Hxx, Hyy, Hxy).
 *
 * Neither the height nor the slope is returned, because nothing downstream
 * wants either: a caustic is a second-derivative effect from end to end. The
 * Hessian is symmetric, so a fourth entry would be a copy of the third.
 */
SHADER_CTX vec3 surfaceCurvature(vec2 p, float t) {
    vec3 total = vec3(0.0f);

    float amp = LC_AMPLITUDE;
    float k = LC_FREQUENCY;
    float bearing = LC_BEARING;
    float stagger = 0.0f;

    for (int i = 0; i < LC_WAVES; i++) {
        vec2 dir = vec2(glsl::cos(bearing), glsl::sin(bearing));

        /* Deep-water dispersion: a wave's angular frequency goes as the square
         * root of its wavenumber, so no two scales ever march in step. Give them
         * all one speed instead and the whole net pulses like a heartbeat. */
        float phase = glsl::dot(dir, p) * k + t * LC_CELERITY * glsl::sqrt(k) + stagger;
        float bend = -amp * k * k * glsl::sin(phase);

        total += vec3(dir.x * dir.x, dir.y * dir.y, dir.x * dir.y) * bend;

        bearing += LC_TURN;
        stagger += LC_STAGGER;
        k *= LC_LACUNARITY;
        amp *= LC_GAIN;
    }

    return total;
}

/*
 * How much brighter the floor is for having that surface above it.
 *
 * Light leaving an area dA of surface arrives on dA * |J| of sand, where J is
 * the Jacobian of the displacement depth * grad(h). For that displacement the
 * determinant works out to a quadratic in depth,
 *
 *     |J|(depth) = 1 + depth * tr(H) + depth^2 * det(H),
 *
 * so the entire web lives in two numbers taken off the Hessian. That is why the
 * three colour channels below cost almost nothing: one field evaluation, read
 * back at three depths. Where |J| reaches zero the sheet has folded onto a line
 * and the brightness is unbounded; the softening term is the only thing between
 * that line and a division by zero, and its size is the width of the filament.
 */
SHADER_CTX float foldBrightness(vec3 curv, float depth) {
    float traceH = curv.x + curv.y;
    float detH = curv.x * curv.y - curv.z * curv.z;
    float squeeze = 1.0f + depth * traceH + depth * depth * detH;
    return 1.0f / (glsl::abs(squeeze) + LC_FOLD_SOFTEN);
}

/*
 * The shoal: a bar running across the frame with a crest that wanders, and a
 * tide breathing over the whole of it. Depth sets the scale of the web — deep
 * water spreads the filaments and swallows the red — so this one number decides
 * most of the picture.
 */
SHADER_CTX float barDepth(vec2 p, float t) {
    float across = glsl::dot(p, vec2(0.15f, 0.99f));
    float crest = across - LC_BAR_LINE - glsl::sin(p.x * 0.85f) * 0.30f -
                  glsl::sin(p.x * 0.33f + 1.7f) * 0.55f;
    float shoal = glsl::smoothstep(-0.95f, 1.15f, crest);
    /* Runnels — the shallow gutters a bar drains through, crossing it at an
     * angle. They are here for the colour: a band of extra depth is a band
     * further along the ramp, and that banding is the gem's. Painting it on as
     * white over the top is the obvious alternative, and it comes out grey,
     * because the one thing white does to a picture this pale is take the
     * colour out of it. */
    float runnel = glsl::sin(glsl::dot(p, vec2(0.92f, -0.39f)) * 1.15f + 0.6f);
    float tide = 1.0f + LC_TIDE * glsl::sin(t * 0.17f);
    return (LC_DEPTH_MIN + LC_DEPTH_SPAN * shoal + LC_RUNNEL * runnel) * tide;
}

/*
 * Ripples the swell leaves in a bar lie across it and wander along their own
 * length; the fine term is grain, and it is the only thing keeping the floor
 * from reading as painted plaster.
 */
SHADER_CTX float sandRelief(vec2 p) {
    float lie = glsl::dot(p, vec2(0.29f, 0.96f)) * LC_RIPPLE + lcNoise(p * 0.55f) * 3.2f;
    float ridges = glsl::sin(lie) * 0.5f + 0.5f;
    return ridges * ridges * 0.7f + lcNoise(p * 34.0f) * 0.3f;
}

SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime) {
    vec2 uv = (fragCoord - iResolution * 0.5f) / iResolution.y;

    /* Where this pixel's ray meets the sand. Pitch is the whole composition: at
     * a right angle the map is isotropic and the web is even edge to edge, and
     * every degree away from that buys recession with evenness. Lean too far
     * and the vertical scale changes several-fold across the frame, which the
     * eye reads as a sunburst of streaks rather than as distance — and the far
     * sand compresses finer than a pixel, which a shader with no derivatives to
     * filter by can answer only with noise. */
    vec3 ray = glsl::normalize(vec3(uv.x, uv.y, LC_LENS));
    float lean = glsl::cos(LC_PITCH);
    float dip = glsl::sin(LC_PITCH);
    vec3 dir = vec3(ray.x, ray.y * lean - ray.z * dip, ray.y * dip + ray.z * lean);
    vec2 p = vec2(dir.x, dir.z) * (-LC_EYE / dir.y);
    p.y -= iTime * LC_DRIFT;

    float depth = barDepth(p, iTime);
    vec3 curv = surfaceCurvature(p, iTime);

    vec3 web = vec3(foldBrightness(curv, depth * LC_DISPERSE_R),
                    foldBrightness(curv, depth),
                    foldBrightness(curv, depth * LC_DISPERSE_B)) *
               LC_WEB_GAIN;

    vec3 albedo = LC_SAND * (0.84f + 0.26f * sandRelief(p));

    /* Beer-Lambert, down and back. Everything that makes this water turquoise
     * rather than grey happens on this line. */
    vec3 transmit = glsl::exp(LC_ABSORB * (-depth * LC_PATH));
    vec3 col = albedo * (web + vec3(LC_AMBIENT)) * transmit;

    /* What the column itself sends back — the light that never reached the
     * sand. Pale over the crest of the bar, turquoise off its flank, petrol
     * blue in the trough beyond.
     *
     * The strength is one number, not three. Weighting it by (1 - transmit) per
     * channel is the obvious thing and it is wrong: that factor is largest
     * exactly where absorption is strongest, so the light the water sends back
     * comes out red. Water scatters blue and absorbs red, and those are two
     * different coefficients. One achromatic factor, with the colour left to
     * the ramp, is the honest cheap version. */
    vec3 body = glsl::mix(LC_SHALLOW, LC_LAGOON, glsl::smoothstep(1.20f, 2.10f, depth));
    body = glsl::mix(body, LC_DEEP, glsl::smoothstep(2.20f, 3.20f, depth));
    float haze = 1.0f - glsl::exp(-depth * LC_PATH * LC_HAZE);
    col += body * (haze * LC_SCATTER);

    /* Clamped, because uv.x grows with the aspect ratio: past about 3.3:1 an
     * unclamped vignette goes negative and the sides of a panoramic render fade
     * to black instead of to the corner of the frame. */
    col *= glsl::max(1.0f - 0.34f * glsl::dot(uv, uv), 0.0f);

    /* Exposure with a shoulder, then gamma. A hard clamp would flatten every
     * filament into the same white and throw away the only thing they differ
     * by, which is how bright the fold is. The shoulder costs saturation on the
     * way, because it pulls the strongest channel down hardest, so the last
     * step puts some back — and that is the whole difference between pale grey
     * water and pale turquoise water. */
    col = vec3(1.0f) - glsl::exp(col * -LC_EXPOSURE);
    float luma = glsl::dot(col, vec3(0.2126f, 0.7152f, 0.0722f));
    col = glsl::mix(vec3(luma), col, LC_SATURATION);
    fragColor = vec4(glsl::pow(glsl::max(col, vec3(0.0f)), vec3(0.4545f)), 1.0f);
}
