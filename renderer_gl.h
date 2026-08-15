#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>
#include <regex>
#include <stdexcept>
#include <sys/stat.h>


typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_FALSE 0
#define GL_TRIANGLE_FAN 0x0006
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601



typedef void (APIENTRY *PFNGLGENBUFFERS)(GLsizei, GLuint*);
typedef void (APIENTRY *PFNGLBINDBUFFER)(GLenum, GLuint);
typedef void (APIENTRY *PFNGLBUFFERDATA)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAY)(GLuint);
typedef void (APIENTRY *PFNGLVERTEXATTRIBPOINTER)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef GLuint (APIENTRY *PFNGLCREATESHADER)(GLenum);
typedef void (APIENTRY *PFNGLSHADERSOURCE)(GLuint, GLsizei, const GLchar**, const GLint*);
typedef void (APIENTRY *PFNGLCOMPILESHADER)(GLuint);
typedef void (APIENTRY *PFNGLGETSHADERIV)(GLuint, GLenum, GLint*);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOG)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAM)(void);
typedef void (APIENTRY *PFNGLATTACHSHADER)(GLuint, GLuint);
typedef void (APIENTRY *PFNGLLINKPROGRAM)(GLuint);
typedef void (APIENTRY *PFNGLGETPROGRAMIV)(GLuint, GLenum, GLint*);
typedef void (APIENTRY *PFNGLGETPROGRAMINFOLOG)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (APIENTRY *PFNGLUSEPROGRAM)(GLuint);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATION)(GLuint, const GLchar*);
typedef void (APIENTRY *PFNGLUNIFORM1F)(GLint, GLfloat);
typedef void (APIENTRY *PFNGLUNIFORM2F)(GLint, GLfloat, GLfloat);
typedef void (APIENTRY *PFNGLUNIFORM1I)(GLint, GLint);
typedef void (APIENTRY *PFNGLGENFRAMEBUFFERS)(GLsizei, GLuint*);
typedef void (APIENTRY *PFNGLBINDFRAMEBUFFER)(GLenum, GLuint);
typedef void (APIENTRY *PFNGLFRAMEBUFFERTEXTURE2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (APIENTRY *PFNGLCHECKFRAMEBUFFERSTATUS)(GLenum);
typedef void (APIENTRY *PFNGLGENVERTEXARRAYS)(GLsizei, GLuint*);
typedef void (APIENTRY *PFNGLBINDVERTEXARRAY)(GLuint);
typedef void (APIENTRY *PFNGLACTIVETEXTURE)(GLenum);

class GlRenderer {
    int width, height;
    SDL_Window* hidden_window;
    SDL_GLContext gl_context;
    GLuint program;
    GLuint fbo, fbo_texture;
    GLuint vbo, vao;
    GLuint user_texture = 0;
    // Tightly packed readback staging, so the row flip costs no per-frame alloc.
    std::vector<uint8_t> readback;

    
    PFNGLGENBUFFERS glGenBuffers = nullptr;
    PFNGLBINDBUFFER glBindBuffer = nullptr;
    PFNGLBUFFERDATA glBufferData = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAY glEnableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTER glVertexAttribPointer = nullptr;
    PFNGLCREATESHADER glCreateShader = nullptr;
    PFNGLSHADERSOURCE glShaderSource = nullptr;
    PFNGLCOMPILESHADER glCompileShader = nullptr;
    PFNGLGETSHADERIV glGetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOG glGetShaderInfoLog = nullptr;
    PFNGLCREATEPROGRAM glCreateProgram = nullptr;
    PFNGLATTACHSHADER glAttachShader = nullptr;
    PFNGLLINKPROGRAM glLinkProgram = nullptr;
    PFNGLGETPROGRAMIV glGetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOG glGetProgramInfoLog = nullptr;
    PFNGLUSEPROGRAM glUseProgram = nullptr;
    PFNGLGETUNIFORMLOCATION glGetUniformLocation = nullptr;
    PFNGLUNIFORM1F glUniform1f = nullptr;
    PFNGLUNIFORM2F glUniform2f = nullptr;
    PFNGLUNIFORM1I glUniform1i = nullptr;
    PFNGLGENFRAMEBUFFERS glGenFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFER glBindFramebuffer = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2D glFramebufferTexture2D = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUS glCheckFramebufferStatus = nullptr;
    PFNGLGENVERTEXARRAYS glGenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAY glBindVertexArray = nullptr;
    PFNGLACTIVETEXTURE glActiveTexture = nullptr;

    void load_functions() {
        auto load = [](const char* name) {
            void* p = (void*)SDL_GL_GetProcAddress(name);
            if (!p) printf("[GL ERROR] Failed to load: %s\n", name);
            return p;
        };
        glGenBuffers = (PFNGLGENBUFFERS)load("glGenBuffers");
        glBindBuffer = (PFNGLBINDBUFFER)load("glBindBuffer");
        glBufferData = (PFNGLBUFFERDATA)load("glBufferData");
        glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAY)load("glEnableVertexAttribArray");
        glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTER)load("glVertexAttribPointer");
        glCreateShader = (PFNGLCREATESHADER)load("glCreateShader");
        glShaderSource = (PFNGLSHADERSOURCE)load("glShaderSource");
        glCompileShader = (PFNGLCOMPILESHADER)load("glCompileShader");
        glGetShaderiv = (PFNGLGETSHADERIV)load("glGetShaderiv");
        glGetShaderInfoLog = (PFNGLGETSHADERINFOLOG)load("glGetShaderInfoLog");
        glCreateProgram = (PFNGLCREATEPROGRAM)load("glCreateProgram");
        glAttachShader = (PFNGLATTACHSHADER)load("glAttachShader");
        glLinkProgram = (PFNGLLINKPROGRAM)load("glLinkProgram");
        glGetProgramiv = (PFNGLGETPROGRAMIV)load("glGetProgramiv");
        glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOG)load("glGetProgramInfoLog");
        glUseProgram = (PFNGLUSEPROGRAM)load("glUseProgram");
        glGetUniformLocation = (PFNGLGETUNIFORMLOCATION)load("glGetUniformLocation");
        glUniform1f = (PFNGLUNIFORM1F)load("glUniform1f");
        glUniform2f = (PFNGLUNIFORM2F)load("glUniform2f");
        glUniform1i = (PFNGLUNIFORM1I)load("glUniform1i");
        glGenFramebuffers = (PFNGLGENFRAMEBUFFERS)load("glGenFramebuffers");
        glBindFramebuffer = (PFNGLBINDFRAMEBUFFER)load("glBindFramebuffer");
        glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2D)load("glFramebufferTexture2D");
        glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUS)load("glCheckFramebufferStatus");
        glGenVertexArrays = (PFNGLGENVERTEXARRAYS)load("glGenVertexArrays");
        glBindVertexArray = (PFNGLBINDVERTEXARRAY)load("glBindVertexArray");
        glActiveTexture = (PFNGLACTIVETEXTURE)load("glActiveTexture");

        if (!glGenBuffers || !glCreateShader || !glGetShaderiv || !glGenVertexArrays) {
            fprintf(stderr, "\n[FATAL] Critical OpenGL functions missing.\n"); exit(1);
        }
    }

    std::string resolvePath(std::string path) {
        struct stat buffer;
        if (stat(path.c_str(), &buffer) == 0) return path;
        std::string up = "../" + path;
        if (stat(up.c_str(), &buffer) == 0) return up;
        if (path.substr(0, 3) == "../") {
            std::string stripped = path.substr(3);
            if (stat(stripped.c_str(), &buffer) == 0) return stripped;
        }
        return "";
    }

    std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
        return str;
    }


    // Counts a character in a line; used to balance braces when skipping a
    // host-only function body.
    static int countChar(const std::string& s, char c) {
        int n = 0;
        for (size_t i = 0; i < s.size(); ++i) if (s[i] == c) ++n;
        return n;
    }

    std::string readFile(std::string path) {
        std::string cleanPath = resolvePath(path);
        if (cleanPath.empty()) {
            // Previously exit(1). A missing shader is the same class of
            // problem as one that will not compile, and killing the process
            // denied the caller the CPU fallback it already knows how to do.
            fail("shader source not found: " + path);
        }

        std::ifstream f(cleanPath);
        std::string content, line;
        int skipDepth = 0;
        while(std::getline(f, line)) {
            // A host-only declaration that opens a block must take its body
            // with it. Dropping only the signature left the body orphaned at
            // program scope, which is what broke mario.cpp and tunnelwisp.cpp:
            // both define `extern "C" vec2 mainSound(...)` for the audio
            // thread, which is host code with no place in a shader.
            if (skipDepth > 0) {
                skipDepth += countChar(line, '{') - countChar(line, '}');
                continue;
            }
            if(line.find("#include") != std::string::npos) continue;
            if(line.find("#pragma") != std::string::npos) continue;
            if(line.find("using namespace") != std::string::npos) continue;
            if(line.find("extern") != std::string::npos) {
                const int opened = countChar(line, '{') - countChar(line, '}');
                if (opened > 0) skipDepth = opened;
                continue;
            }
            
            line = replaceAll(line, "inline ", "");
            // Namespace-qualified calls are valid C++ and a syntax error in
            // GLSL. renderer_metal.mm has always stripped these; this copy of
            // the transpiler had drifted behind it, so any shader written as
            // glsl::length(...) failed to compile here while working on Metal.
            line = replaceAll(line, "glsl::", "");
            line = replaceAll(line, "sumi::", "");
            // Any surviving `::` is the global-scope qualifier —
            // tunnelwisp.cpp calls ::tanhf to reach libc past libsumi's
            // overload. GLSL has no scope resolution and no single `:` is
            // touched, so stripping the pair is safe in this subset.
            line = replaceAll(line, "::", "");

            // C++ reference parameters become GLSL parameter qualifiers.
            //
            // fragColor maps to `out` — every shader fills it and none reads
            // it, and main() below passes a variable it has just declared.
            //
            // Everything else maps to `inout`, preserving C++ reference
            // semantics. `out` would be wrong: mario.cpp's sprite helpers take
            // `vec3& color` and composite onto what is already there, so `out`
            // would make each sprite erase the background.
            //
            // Only the `vec4 &` spellings were handled before, so seascape,
            // lunar, mario, and rainforest — which pass vec3 and float by
            // reference — emitted a bare `&` and failed to compile. Metal has
            // covered these since it was written; this copy had drifted.
            static const std::string refTypes =
                "float|int|bool|vec2|vec3|vec4|ivec2|ivec3|ivec4|"
                "bvec2|bvec3|bvec4|mat2|mat3|mat4";
            static const std::regex fragColorRef("\\b(" + refTypes + ")\\s*&\\s*fragColor\\b");
            static const std::regex generalRef("\\b(" + refTypes + ")\\s*&\\s*");

            // A const reference is a read-only input, so it maps to `in`,
            // and must be matched before the general rule: leaving the const
            // and qualifying it `inout` yields `const inout vec4`, which GLSL
            // rejects. rainforest.cpp's yzw() and yz() take const refs.
            static const std::regex constRef("\\bconst\\s+(" + refTypes + ")\\s*&\\s*");

            line = std::regex_replace(line, fragColorRef, "out $1 fragColor");
            line = std::regex_replace(line, constRef, "in $1 ");
            line = std::regex_replace(line, generalRef, "inout $1 ");

            // `(void)x;` suppresses an unused-parameter warning on the CPU
            // build and is a syntax error in GLSL. MSL is C++-based and
            // accepts it, so this is GLSL-only. Used by mario, rainforest,
            // and tunnelwisp.
            static const std::regex voidCast("\\(\\s*void\\s*\\)\\s*[A-Za-z_][A-Za-z0-9_]*\\s*;");
            line = std::regex_replace(line, voidCast, "");

            // C++ direct-initialization — `vec2 c(0.0f, 1.0f);` — is a
            // declaration in C++ and a syntax error in GLSL, which wants an
            // explicit constructor call. Anchored to an indented statement
            // ending in `);` so it cannot match a function definition, whose
            // line ends in `{`. lunar.cpp uses it twice.
            static const std::regex directInit(
                "^([ \\t]+)(vec2|vec3|vec4|mat2|mat3|mat4|ivec2|ivec3|ivec4|bvec2|bvec3|bvec4)"
                "[ \\t]+([A-Za-z_][A-Za-z0-9_]*)[ \\t]*\\((.*)\\)[ \\t]*;[ \\t]*$");
            line = std::regex_replace(line, directInit, "$1$2 $3 = $2($4);");

            // C-style casts — `(float)i` — are a GLSL syntax error; GLSL
            // spells it as a constructor call, `float(i)`. Limited to a cast
            // of a bare identifier or numeric literal, which covers every use
            // in the corpus.
            // Two forms. The parenthesised one is easier: in
            // `(float)((i >> 1) & 1)` the existing parentheses already serve
            // as the constructor's argument list, so dropping the cast's own
            // parentheses is the whole transformation.
            static const std::regex cCastParen("\\(\\s*(float|int|uint|bool)\\s*\\)\\s*\\(");
            line = std::regex_replace(line, cCastParen, "$1(");

            static const std::regex cCast(
                "\\(\\s*(float|int|uint|bool)\\s*\\)\\s*([A-Za-z_][A-Za-z0-9_]*|[0-9]+\\.?[0-9]*f?)");
            line = std::regex_replace(line, cCast, "$1($2)");

            // noise1..noise4 are reserved GLSL built-ins returning genType, so
            // a shader defining its own — aurora.cpp declares
            // `float noise2(vec2)` — is a return-type redeclaration and will
            // not compile. Renaming definition and call sites together keeps
            // the shader self-consistent; the built-ins are deprecated,
            // removed from core profiles, and return 0 on most drivers.
            static const std::regex reservedNoise("\\bnoise([1-4])\\b");
            line = std::regex_replace(line, reservedNoise, "eshi_noise$1");
            line = replaceAll(line, "if (iChannel0.data == nullptr)", "if (false)");

            // libsumi exposes swizzles as calls; GLSL wants members. This was
            // a hardcoded list of eight spellings and silently missed the
            // rest — lunar.cpp's .xz() among them. Metal has used the general
            // form since it was written; this is the same drift as the missing
            // glsl:: strip.
            line = std::regex_replace(line, std::regex("\\.([xyzw]{1,4})\\(\\)"), ".$1");

            content += line + "\n";
        }
        return content;
    }

    // Releases the context and window, then throws.
    //
    // A destructor never runs for an object whose constructor threw, so this
    // is the only chance to hand these back before main.cpp catches and falls
    // through to the CPU renderer. Same contract MetalRenderer already uses.
    void fail(const std::string& message) {
        if (gl_context) { SDL_GL_DeleteContext(gl_context); gl_context = nullptr; }
        if (hidden_window) { SDL_DestroyWindow(hidden_window); hidden_window = nullptr; }
        throw std::runtime_error(message);
    }

    bool shaderCompiled(GLuint shader, std::string& log_out) {
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success) return true;
        char infoLog[2048];
        glGetShaderInfoLog(shader, (GLsizei)sizeof(infoLog), NULL, infoLog);
        log_out = infoLog;
        return false;
    }

    bool programLinked(GLuint prog, std::string& log_out) {
        GLint success = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &success);
        if (success) return true;
        char infoLog[2048];
        glGetProgramInfoLog(prog, (GLsizei)sizeof(infoLog), NULL, infoLog);
        log_out = infoLog;
        return false;
    }

public:
    GlRenderer(int w, int h, const char* shaderPath, float* texData=nullptr, int texW=0, int texH=0) : width(w), height(h) {
        if(!SDL_WasInit(SDL_INIT_VIDEO)) SDL_Init(SDL_INIT_VIDEO);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        hidden_window = SDL_CreateWindow("EshiGL", 0, 0, 10, 10, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        gl_context = SDL_GL_CreateContext(hidden_window);
        load_functions();
        
        if(texData) {
            glGenTextures(1, &user_texture);
            glBindTexture(GL_TEXTURE_2D, user_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGBA, GL_FLOAT, texData);
        }

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        float quadVertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        printf("[GL] Compiling: %s\n", shaderPath);
        const char* vsSrc = "#version 330 core\nlayout (location = 0) in vec2 aPos; void main(){ gl_Position = vec4(aPos, 0.0, 1.0); }";
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vsSrc, NULL);
        glCompileShader(vs);

        std::string compileLog;
        if (!shaderCompiled(vs, compileLog)) {
            fail("vertex shader compilation failed:\n" + compileLog);
        }

        std::string userCode = readFile(shaderPath);
        
        
        std::string fsSrc = "#version 330 core\n"
                            "out vec4 FragColor;\n"
                            "uniform vec2 iResolution;\n"
                            "uniform float iTime;\n"
                            "uniform sampler2D iChannel0;\n"
                            "\n"
                            "// --- Compatibility Wrapper ---\n"
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
                            "#define exp2f exp2\n"
                            "#define logf log\n"
                            "#define log2f log2\n"
                            "#define sqrtf sqrt\n"
                            "#define fabsf abs\n"
                            "#define floorf floor\n"
                            "#define ceilf ceil\n"
                            "#define roundf round\n"
                            "#define truncf trunc\n"
                            "#define tanhf tanh\n"
                            "#define modf mod\n"
                            "#define fminf min\n"
                            "#define fmaxf max\n"
                            "#define glsl_core_h\n"
                            "\n"
                            + userCode + "\n"
                            "\n"
                            "void main() { vec4 col; mainImage(col, gl_FragCoord.xy, iResolution, iTime); FragColor = col; }";
        
        const char* fsSrcPtr = fsSrc.c_str();
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fsSrcPtr, NULL);
        glCompileShader(fs);
        if (!shaderCompiled(fs, compileLog)) {
            // The transpiled GLSL is generated, not authored, so the line
            // numbers in the log refer to text the user cannot open. Name the
            // source file so the message is actionable.
            fail("fragment shader compilation failed for " + std::string(shaderPath) +
                 ":\n" + compileLog);
        }

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        if (!programLinked(program, compileLog)) {
            fail("shader program link failed for " + std::string(shaderPath) +
                 ":\n" + compileLog);
        }
        glUseProgram(program);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &fbo_texture);
        glBindTexture(GL_TEXTURE_2D, fbo_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            fail("framebuffer incomplete");
        }
        printf("[GL] Ready.\n");
    }

    ~GlRenderer() {
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(hidden_window);
    }

    void renderFrame(uint8_t* pixelBuffer, int stride, float time) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height);

        glUseProgram(program);
        glUniform2f(glGetUniformLocation(program, "iResolution"), (float)width, (float)height);
        glUniform1f(glGetUniformLocation(program, "iTime"), time);
        
        if(user_texture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, user_texture);
            glUniform1i(glGetUniformLocation(program, "iChannel0"), 0);
        }

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // Read back into scratch, then copy out row by row in reverse.
        //
        // Three things this has to get right, all of which the previous
        // one-line readback got wrong:
        //
        //  1. Orientation. glReadPixels returns rows bottom-up — offset 0 is
        //     the row with the smallest gl_FragCoord.y — while every consumer
        //     here is top-down, matching CpuRenderer, which writes row 0 from
        //     the largest fragCoord.y. Copying straight through mirrored the
        //     image vertically against the CPU renderer.
        //  2. Format. GL_RGB packs three bytes per pixel, but Display uses
        //     SDL_PIXELFORMAT_RGBA32 and SimpleEncoder uses AV_PIX_FMT_RGBA,
        //     both four. The short rows were reinterpreted as four-byte ones,
        //     which skews the image and shifts the channels.
        //  3. Stride. The destination pitch was ignored entirely; FFmpeg
        //     aligns linesize, so it is not always width * 4.
        //
        // These survived because USE_OPENGL is only defined by
        // scripts/build.arm64.bat, so neither the Makefile nor build.zig ever
        // compiled this path. The Larimar port of this backend
        // (core/src/render/gl.cpp) carries the same fix; a pixel diff against
        // the CPU tier is what surfaced it.
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        const size_t packed_stride = (size_t)width * 4;
        const size_t needed = packed_stride * (size_t)height;
        if (readback.size() != needed) readback.resize(needed);

        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, readback.data());

        for (int y = 0; y < height; ++y) {
            memcpy(pixelBuffer + (size_t)y * (size_t)stride,
                   readback.data() + (size_t)(height - 1 - y) * packed_stride,
                   packed_stride);
        }
    }
};
