/**
 * @file transpile.cpp
 * @brief Implementation of the C++ shader subset -> GLSL / MSL.
 *
 * Consolidated from renderer_gl.h and renderer_metal.mm, which each carried
 * their own copy of these substitutions. Where the two disagreed, the union is
 * taken and the difference is noted at the rule.
 */
#include "transpile.h"

#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <sys/stat.h>

namespace eshi {
namespace transpile {

namespace {

bool file_exists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFREG) != 0;
}

/* Mirrors the search the old renderers did, so paths that worked still work. */
std::string resolve_path(const std::string& path) {
    const std::string candidates[] = {
        path,
        "../" + path,
        "../../" + path,
    };
    for (int i = 0; i < 3; ++i) {
        if (file_exists(candidates[i])) return candidates[i];
    }
    /* A leading ../ that the caller already added and we should try without. */
    if (path.size() > 3 && path.compare(0, 3, "../") == 0) {
        const std::string stripped = path.substr(3);
        if (file_exists(stripped)) return stripped;
    }
    return "";
}

std::string replace_all(std::string str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

/**
 * Lines that exist for the CPU compiler and have no meaning in a shader.
 *
 * This is the bluntest part of the subset and the first thing a real parser
 * would replace: a shader may not use #include, and any `extern` declaration is
 * assumed to be host plumbing.
 */
bool is_host_only_line(const std::string& line) {
    return line.find("#include") != std::string::npos ||
           line.find("#pragma") != std::string::npos ||
           line.find("using namespace") != std::string::npos ||
           line.find("extern") != std::string::npos;
}

int count_char(const std::string& s, char c) {
    int n = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == c) ++n;
    }
    return n;
}

/** Substitutions common to both targets. */
std::string common_rules(std::string line) {
    line = replace_all(line, "inline ", "");
    line = replace_all(line, "glsl::", "");
    line = replace_all(line, "sumi::", "");
    line = replace_all(line, "if (iChannel0.data == nullptr)", "if (false)");
    /*
     * Any surviving `::` is the global-scope qualifier — tunnelwisp.cpp calls
     * `::tanhf` to reach libc past libsumi's overload. Neither shading language
     * has scope resolution, and no single `:` is affected, so stripping the
     * pair is safe within this subset.
     */
    line = replace_all(line, "::", "");
    /* libsumi exposes swizzles as calls; both shading languages want members. */
    return std::regex_replace(line, std::regex("\\.([xyzw]{1,4})\\(\\)"), ".$1");
}

/** Types that may legally appear as a reference parameter in the subset. */
const char* kRefTypes =
    "float|int|bool|vec2|vec3|vec4|ivec2|ivec3|ivec4|bvec2|bvec3|bvec4|mat2|mat3|mat4";

std::string glsl_rules(std::string line) {
    /*
     * C++ reference parameters become GLSL parameter qualifiers.
     *
     * fragColor maps to `out`: every shader in the corpus fills it and none
     * reads it, and the generated entry point passes a variable it has just
     * declared, which an `inout` would read before assignment.
     *
     * Everything else maps to `inout`, which preserves C++ reference semantics
     * exactly. `out` would be actively wrong here — mario.cpp's sprite helpers
     * take `vec3& color` and composite onto what is already there, so an `out`
     * qualifier would make every sprite erase the background instead of
     * drawing over it. A transpiler cannot assume write-before-read.
     *
     * This previously handled only the three spellings of `vec4 &`, so
     * seascape, lunar, mario, and rainforest — which pass vec3 and float by
     * reference — emitted a bare `&` and failed to compile.
     */
    static const std::regex frag_color_ref(
        std::string("\\b(") + kRefTypes + ")\\s*&\\s*fragColor\\b");
    static const std::regex general_ref(
        std::string("\\b(") + kRefTypes + ")\\s*&\\s*");

    /*
     * A const reference is a read-only input, so it maps to `in`. It must be
     * matched before the general rule: leaving the `const` in place and
     * qualifying it `inout` produces `const inout vec4`, which GLSL rejects
     * outright. rainforest.cpp's yzw() and yz() helpers take const refs.
     */
    static const std::regex const_ref(
        std::string("\\bconst\\s+(") + kRefTypes + ")\\s*&\\s*");

    line = std::regex_replace(line, frag_color_ref, "out $1 fragColor");
    line = std::regex_replace(line, const_ref, "in $1 ");
    line = std::regex_replace(line, general_ref, "inout $1 ");

    /*
     * `(void)x;` suppresses an unused-parameter warning on the CPU build and
     * has no GLSL equivalent — the cast is a syntax error there. MSL is
     * C++-based and accepts it, which is why this is a GLSL-only rule.
     * mario.cpp, rainforest.cpp, and tunnelwisp.cpp all use it.
     */
    static const std::regex void_cast("\\(\\s*void\\s*\\)\\s*[A-Za-z_][A-Za-z0-9_]*\\s*;");
    line = std::regex_replace(line, void_cast, "");

    /*
     * C++ direct-initialization — `vec2 c(0.0f, 1.0f);` — is a declaration in
     * C++ but a syntax error in GLSL, which requires an explicit constructor
     * call. Again valid in MSL, so GLSL-only. lunar.cpp uses it twice.
     *
     * Anchored to an indented statement ending in `);` so it cannot match a
     * function definition, whose line ends in `{`, or a file-scope prototype.
     */
    static const std::regex direct_init(
        "^([ \\t]+)(vec2|vec3|vec4|mat2|mat3|mat4|ivec2|ivec3|ivec4|bvec2|bvec3|bvec4)"
        "[ \\t]+([A-Za-z_][A-Za-z0-9_]*)[ \\t]*\\((.*)\\)[ \\t]*;[ \\t]*$");
    line = std::regex_replace(line, direct_init, "$1$2 $3 = $2($4);");

    /*
     * C-style casts — `(float)i` — are a GLSL syntax error; GLSL spells the
     * same thing as a constructor call, `float(i)`. Valid in MSL, so
     * GLSL-only.
     *
     * Two forms, and the parenthesised one is the easier of the two: in
     * `(float)((i >> 1) & 1)` the existing parentheses already serve as the
     * constructor's argument list, so dropping the cast's own parentheses is
     * the whole transformation — no matching-paren search needed.
     */
    static const std::regex c_cast_paren("\\(\\s*(float|int|uint|bool)\\s*\\)\\s*\\(");
    line = std::regex_replace(line, c_cast_paren, "$1(");

    static const std::regex c_cast_simple(
        "\\(\\s*(float|int|uint|bool)\\s*\\)\\s*([A-Za-z_][A-Za-z0-9_]*|[0-9]+\\.?[0-9]*f?)");
    line = std::regex_replace(line, c_cast_simple, "$1($2)");

    /*
     * noise1..noise4 are reserved GLSL built-ins returning genType, so a
     * shader that defines its own — aurora.cpp declares `float noise2(vec2)` —
     * is a return-type redeclaration and will not compile. MSL has no such
     * names, which is why this is GLSL-only.
     *
     * Renaming definition and call sites together keeps the shader
     * self-consistent. Nothing is lost: the built-ins are deprecated, removed
     * from core profiles, and return 0 on most drivers anyway.
     */
    static const std::regex reserved_noise("\\bnoise([1-4])\\b");
    line = std::regex_replace(line, reserved_noise, "eshi_noise$1");

    return line;
}

std::string msl_rules(std::string line, int uniform_floats) {
    /* MSL requires an explicit address space on every reference parameter. */
    line = replace_all(line, "float&", "thread float&");
    line = replace_all(line, "float &", "thread float& ");
    line = replace_all(line, "vec2&", "thread vec2&");
    line = replace_all(line, "vec2 &", "thread vec2& ");
    line = replace_all(line, "vec3&", "thread vec3&");
    line = replace_all(line, "vec3 &", "thread vec3& ");
    line = replace_all(line, "vec4&", "thread vec4&");
    line = replace_all(line, "vec4 &", "thread vec4& ");

    /* MSL has tanh natively; a shader that defines its own would collide. */
    line = std::regex_replace(
        line, std::regex("SHADER_CTX\\s+vec4\\s+tanh\\(vec4\\s+v\\)\\s*\\{"),
        "vec4 __eshi_unused_tanh(vec4 v) {");
    line = replace_all(line, "::tanhf", "tanh");

    line = replace_all(line, "texture(iChannel0,", "iChannel0.sample(smp,");
    line = replace_all(line, "texture(::iChannel0,", "iChannel0.sample(smp,");

    /*
     * MSL has no global bindings: the texture, sampler, and uniform block are
     * kernel arguments and must be threaded through mainImage explicitly. GLSL
     * declares the same things at global scope, which is why only this target
     * needs the rewrite.
     */
    if (line.find("mainImage") != std::string::npos) {
        std::string extra = ", texture2d<float, access::sample> iChannel0, sampler smp";
        if (uniform_floats > 0) extra += ", constant float* eshi_uniforms";
        line = std::regex_replace(line, std::regex("\\)\\s*\\{"), extra + ") {");
    }
    return line;
}

/** The libc float-suffix intrinsics both languages spell without the suffix. */
const char* kMathPreamble =
    "#define SUMI_CTX \n"
    "#define SHADER_CTX \n"
    "#define M_PI 3.14159265359\n"
    "#define sinf sin\n"
    "#define cosf cos\n"
    "#define tanf tan\n"
    "#define asinf asin\n"
    "#define acosf acos\n"
    "#define atanf atan\n"
    "#define powf pow\n"
    "#define expf exp\n"
    "#define exp2f exp2\n"
    "#define logf log\n"
    "#define log2f log2\n"
    "#define sqrtf sqrt\n"
    "#define fabsf abs\n"
    "#define floorf floor\n"
    "#define ceilf ceil\n"
    "#define roundf round\n"
    "#define truncf trunc\n"
    "#define sinhf sinh\n"
    "#define coshf cosh\n"
    "#define tanhf tanh\n"
    "#define fminf min\n"
    "#define fmaxf max\n"
    "#define glsl_core_h\n";

/*
 * atan2f cannot live in the shared preamble: GLSL's atan is overloaded for one
 * and two arguments, while MSL splits them into atan and atan2. Mapping it to
 * plain `atan` for both is what broke polar.cpp on Metal.
 */
const char* kGlslMathPreamble = "#define atan2f atan\n";
const char* kMslMathPreamble =
    "#define atan2f atan2\n"
    /*
     * GLSL spells the modulo builtin `mod`; MSL only has `fmod`. Shaders
     * written against the GLSL/libsumi spelling — mario.cpp among them — fail
     * without this. `modf` is a distinct preprocessor token, so the two
     * defines do not interfere.
     */
    "#define mod fmod\n"
    "#define modf fmod\n"
    /*
     * libsumi and GLSL overload atan for one and two arguments; MSL provides
     * only the one-argument form, plus a separately named atan2. polar.cpp
     * calls the two-argument form directly rather than through atan2f, so a
     * define cannot reach it — the fix has to depend on arity.
     *
     * File-scope overloads restore the GLSL spelling without parsing call
     * sites. They do not hide metal::atan: the arities differ, so both land in
     * one overload set.
     */
    "static inline float  atan(float  y, float  x) { return atan2(y, x); }\n"
    "static inline float2 atan(float2 y, float2 x) { return atan2(y, x); }\n"
    "static inline float3 atan(float3 y, float3 x) { return atan2(y, x); }\n"
    "static inline float4 atan(float4 y, float4 x) { return atan2(y, x); }\n";

std::string read_and_convert(const std::string& resolved, Target target, int uniform_floats) {
    std::ifstream file(resolved.c_str());
    std::string body, line;
    int skip_depth = 0;

    while (std::getline(file, line)) {
        /*
         * A host-only declaration that opens a block has to take its body with
         * it. Dropping only the signature line left the body orphaned at
         * program scope — which is what broke mario.cpp and tunnelwisp.cpp,
         * both of which define `extern "C" vec2 mainSound(...)` for the audio
         * thread. That is host code and has no business in a shader at all.
         */
        if (skip_depth > 0) {
            skip_depth += count_char(line, '{') - count_char(line, '}');
            continue;
        }
        if (is_host_only_line(line)) {
            const int opened = count_char(line, '{') - count_char(line, '}');
            if (opened > 0) skip_depth = opened;
            continue;
        }
        line = common_rules(line);
        line = (target == kTargetGlsl) ? glsl_rules(line) : msl_rules(line, uniform_floats);
        body += line;
        body += "\n";
    }

    if (target == kTargetMsl) {
        /* `const` at file scope is `constant` in MSL. */
        body = replace_all(body, "\nconst ", "\nconstant ");
    }
    return body;
}

std::string uniform_binding(Target target, int uniform_floats) {
    if (uniform_floats <= 0) return "";
    std::ostringstream out;
    if (target == kTargetGlsl) {
        out << "uniform float eshi_uniforms[" << uniform_floats << "];\n";
    }
    /* MSL binds the block as a kernel argument; declared at the entry point. */
    return out.str();
}

} /* namespace */

bool build_program(const std::string& path,
                   Target target,
                   int uniform_floats,
                   std::string* out_source,
                   std::string* out_error) {
    const std::string resolved = resolve_path(path);
    if (resolved.empty()) {
        if (out_error) *out_error = "shader source not found: " + path;
        return false;
    }

    const std::string body = read_and_convert(resolved, target, uniform_floats);
    std::ostringstream out;

    if (target == kTargetGlsl) {
        out << "#version 330 core\n"
            << "out vec4 FragColor;\n"
            << "uniform vec2 iResolution;\n"
            << "uniform float iTime;\n"
            << "uniform sampler2D iChannel0;\n"
            << uniform_binding(target, uniform_floats)
            << kMathPreamble
            << kGlslMathPreamble
            << body
            << "\nvoid main() {\n"
            << "    vec2 fragCoord = gl_FragCoord.xy;\n"
            << "    vec4 color = vec4(0.0);\n"
            << "    mainImage(color, fragCoord, iResolution, iTime);\n"
            << "    FragColor = color;\n"
            << "}\n";
    } else {
        const bool has_uniforms = uniform_floats > 0;
        out << "#include <metal_stdlib>\n"
            << "using namespace metal;\n"
            << "typedef float2 vec2;\n"
            << "typedef float3 vec3;\n"
            << "typedef float4 vec4;\n"
            << "typedef float2x2 mat2;\n"
            << "typedef float3x3 mat3;\n"
            << "typedef float4x4 mat4;\n"
            << kMathPreamble
            << kMslMathPreamble
            << "constexpr sampler smp(coord::normalized, address::repeat, filter::linear);\n"
            << body
            << "\nkernel void eshi_main(\n"
            << "    texture2d<float, access::write> outTexture [[texture(0)]],\n"
            << "    texture2d<float, access::sample> iChannel0 [[texture(1)]],\n"
            << "    constant float& iTime [[buffer(0)]],\n";
        if (has_uniforms) {
            out << "    constant float* eshi_uniforms [[buffer(1)]],\n";
        }
        out << "    uint2 gid [[thread_position_in_grid]]) {\n"
            << "    if (gid.x >= outTexture.get_width() || gid.y >= outTexture.get_height()) return;\n"
            << "    vec2 iResolution = vec2(outTexture.get_width(), outTexture.get_height());\n"
            << "    vec2 fragCoord = vec2(float(gid.x) + 0.5, float(outTexture.get_height() - 1 - gid.y) + 0.5);\n"
            << "    vec4 fragColor = vec4(0.0);\n"
            << "    mainImage(fragColor, fragCoord, iResolution, iTime, iChannel0, smp"
            << (has_uniforms ? ", eshi_uniforms" : "") << ");\n"
            << "    outTexture.write(fragColor, gid);\n"
            << "}\n";
    }

    if (out_source) *out_source = out.str();
    return true;
}

} /* namespace transpile */
} /* namespace eshi */
