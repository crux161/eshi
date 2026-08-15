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

/** Substitutions common to both targets. */
std::string common_rules(std::string line) {
    line = replace_all(line, "inline ", "");
    line = replace_all(line, "glsl::", "");
    line = replace_all(line, "sumi::", "");
    line = replace_all(line, "if (iChannel0.data == nullptr)", "if (false)");
    /* libsumi exposes swizzles as calls; both shading languages want members. */
    return std::regex_replace(line, std::regex("\\.([xyzw]{1,4})\\(\\)"), ".$1");
}

std::string glsl_rules(std::string line) {
    /* An out-parameter in C++ is an `out` qualifier in GLSL. */
    line = replace_all(line, "vec4 &fragColor", "out vec4 fragColor");
    line = replace_all(line, "vec4 &", "out vec4 ");
    line = replace_all(line, "vec4& ", "out vec4 ");
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
    "#define atan2f atan\n"
    "#define powf pow\n"
    "#define expf exp\n"
    "#define logf log\n"
    "#define sqrtf sqrt\n"
    "#define fabsf abs\n"
    "#define floorf floor\n"
    "#define ceilf ceil\n"
    "#define fminf min\n"
    "#define fmaxf max\n"
    "#define glsl_core_h\n";

std::string read_and_convert(const std::string& resolved, Target target, int uniform_floats) {
    std::ifstream file(resolved.c_str());
    std::string body, line;

    while (std::getline(file, line)) {
        if (is_host_only_line(line)) continue;
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
            << "#define modf fmod\n"
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
