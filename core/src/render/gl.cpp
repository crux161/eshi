/**
 * @file gl.cpp
 * @brief Kantei Grade 2 (Paper) — OpenGL 3.3 fullscreen-quad backend.
 *
 * Salvaged from renderer_gl.h, with one structural change that mattered: the
 * original created its own hidden SDL window and called SDL_GL_GetProcAddress.
 * The core may not do that — it links no windowing library, and `zig build
 * test` enforces it by linking core/ alone.
 *
 * So the dependency is inverted. The host already owns a GL context; it passes
 * its loader in through eshi_gl_set_proc_loader() and guarantees a current
 * context on every eshi_render() call. That is the §5 boundary rule applied to
 * OpenGL, and it is also what will let the Flutter embedder drive this backend
 * without an SDL window existing at all.
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../eshi_internal.h"
#include "transpile.h"

/* Minimal GL typedefs and enums, so the core needs no GL headers at all. */
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int          GLint;
typedef int          GLsizei;
typedef float        GLfloat;
typedef char         GLchar;
typedef unsigned char GLboolean;
typedef ptrdiff_t    GLsizeiptr;
typedef void         GLvoid;

#define GL_COMPILE_STATUS      0x8B81
#define GL_LINK_STATUS         0x8B82
#define GL_FRAGMENT_SHADER     0x8B30
#define GL_VERTEX_SHADER       0x8B31
#define GL_ARRAY_BUFFER        0x8892
#define GL_STATIC_DRAW         0x88E4
#define GL_FRAMEBUFFER         0x8D40
#define GL_COLOR_ATTACHMENT0   0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TEXTURE_2D          0x0DE1
#define GL_RGBA                0x1908
#define GL_UNSIGNED_BYTE       0x1401
#define GL_FLOAT               0x1406
#define GL_FALSE               0
#define GL_TRIANGLE_FAN        0x0006
#define GL_PACK_ALIGNMENT      0x0D05
#define GL_TEXTURE0            0x84C0
#define GL_TEXTURE_MIN_FILTER  0x2801
#define GL_TEXTURE_MAG_FILTER  0x2800
#define GL_LINEAR              0x2601

namespace {

struct GlFunctions {
    void  (*GenBuffers)(GLsizei, GLuint*);
    void  (*BindBuffer)(GLenum, GLuint);
    void  (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
    void  (*EnableVertexAttribArray)(GLuint);
    void  (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
    GLuint (*CreateShader)(GLenum);
    void  (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    void  (*CompileShader)(GLuint);
    void  (*GetShaderiv)(GLuint, GLenum, GLint*);
    void  (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    GLuint (*CreateProgram)(void);
    void  (*AttachShader)(GLuint, GLuint);
    void  (*LinkProgram)(GLuint);
    void  (*GetProgramiv)(GLuint, GLenum, GLint*);
    void  (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void  (*UseProgram)(GLuint);
    GLint (*GetUniformLocation)(GLuint, const GLchar*);
    void  (*Uniform1f)(GLint, GLfloat);
    void  (*Uniform2f)(GLint, GLfloat, GLfloat);
    void  (*Uniform1i)(GLint, GLint);
    void  (*Uniform1fv)(GLint, GLsizei, const GLfloat*);
    void  (*GenFramebuffers)(GLsizei, GLuint*);
    void  (*BindFramebuffer)(GLenum, GLuint);
    void  (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    GLenum (*CheckFramebufferStatus)(GLenum);
    void  (*GenVertexArrays)(GLsizei, GLuint*);
    void  (*BindVertexArray)(GLuint);
    void  (*GenTextures)(GLsizei, GLuint*);
    void  (*BindTexture)(GLenum, GLuint);
    void  (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
    void  (*TexParameteri)(GLenum, GLenum, GLint);
    void  (*Viewport)(GLint, GLint, GLsizei, GLsizei);
    void  (*DrawArrays)(GLenum, GLint, GLsizei);
    void  (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
    void  (*PixelStorei)(GLenum, GLint);
    void  (*DeleteProgram)(GLuint);
};

struct GlBackend {
    int32_t     width;
    int32_t     height;
    GlFunctions gl;
    GLuint      program;
    GLuint      fbo;
    GLuint      fbo_texture;
    GLuint      vbo;
    GLuint      vao;
    GLint       loc_resolution;
    GLint       loc_time;
    GLint       loc_uniforms;
    /* Tightly packed readback staging, so the row flip needs no per-frame alloc. */
    std::vector<uint8_t> scratch;
};

/* Set through the public eshi_gl_set_proc_loader(). */
EshiGlProcLoader g_proc_loader = NULL;

bool load_functions(GlFunctions* gl, EshiGlProcLoader load) {
    bool ok = true;
    struct Entry { void** slot; const char* name; };
    const Entry entries[] = {
        { (void**)&gl->GenBuffers, "glGenBuffers" },
        { (void**)&gl->BindBuffer, "glBindBuffer" },
        { (void**)&gl->BufferData, "glBufferData" },
        { (void**)&gl->EnableVertexAttribArray, "glEnableVertexAttribArray" },
        { (void**)&gl->VertexAttribPointer, "glVertexAttribPointer" },
        { (void**)&gl->CreateShader, "glCreateShader" },
        { (void**)&gl->ShaderSource, "glShaderSource" },
        { (void**)&gl->CompileShader, "glCompileShader" },
        { (void**)&gl->GetShaderiv, "glGetShaderiv" },
        { (void**)&gl->GetShaderInfoLog, "glGetShaderInfoLog" },
        { (void**)&gl->CreateProgram, "glCreateProgram" },
        { (void**)&gl->AttachShader, "glAttachShader" },
        { (void**)&gl->LinkProgram, "glLinkProgram" },
        { (void**)&gl->GetProgramiv, "glGetProgramiv" },
        { (void**)&gl->GetProgramInfoLog, "glGetProgramInfoLog" },
        { (void**)&gl->UseProgram, "glUseProgram" },
        { (void**)&gl->GetUniformLocation, "glGetUniformLocation" },
        { (void**)&gl->Uniform1f, "glUniform1f" },
        { (void**)&gl->Uniform2f, "glUniform2f" },
        { (void**)&gl->Uniform1i, "glUniform1i" },
        { (void**)&gl->Uniform1fv, "glUniform1fv" },
        { (void**)&gl->GenFramebuffers, "glGenFramebuffers" },
        { (void**)&gl->BindFramebuffer, "glBindFramebuffer" },
        { (void**)&gl->FramebufferTexture2D, "glFramebufferTexture2D" },
        { (void**)&gl->CheckFramebufferStatus, "glCheckFramebufferStatus" },
        { (void**)&gl->GenVertexArrays, "glGenVertexArrays" },
        { (void**)&gl->BindVertexArray, "glBindVertexArray" },
        { (void**)&gl->GenTextures, "glGenTextures" },
        { (void**)&gl->BindTexture, "glBindTexture" },
        { (void**)&gl->TexImage2D, "glTexImage2D" },
        { (void**)&gl->TexParameteri, "glTexParameteri" },
        { (void**)&gl->Viewport, "glViewport" },
        { (void**)&gl->DrawArrays, "glDrawArrays" },
        { (void**)&gl->ReadPixels, "glReadPixels" },
        { (void**)&gl->PixelStorei, "glPixelStorei" },
        { (void**)&gl->DeleteProgram, "glDeleteProgram" },
    };

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        *entries[i].slot = load(entries[i].name);
        if (!*entries[i].slot) {
            std::fprintf(stderr, "[eshi/gl] missing entry point: %s\n", entries[i].name);
            ok = false;
        }
    }
    return ok;
}

bool compile_stage(const GlFunctions& gl, GLuint shader, const char* what) {
    gl.CompileShader(shader);
    GLint success = 0;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success) return true;

    char log[2048];
    gl.GetShaderInfoLog(shader, (GLsizei)sizeof(log), NULL, log);
    std::fprintf(stderr, "[eshi/gl] %s compilation failed:\n%s\n", what, log);
    return false;
}

int gl_available(void) { return g_proc_loader != NULL ? 1 : 0; }

EshiBackend* gl_create(int32_t width, int32_t height,
                       const char* source_path, const char* /*package_path*/) {
    if (!g_proc_loader) {
        std::fprintf(stderr,
                     "[eshi/gl] no proc loader; the host must call "
                     "eshi_gl_set_proc_loader() with a current context\n");
        return NULL;
    }
    if (!source_path) {
        std::fprintf(stderr,
                     "[eshi/gl] material has no source_path; GPU tiers cannot "
                     "execute a compiled-in CPU shader\n");
        return NULL;
    }

    const int kUniformFloats = 64;

    std::string fragment_source, error;
    if (!eshi::transpile::build_program(source_path, eshi::transpile::kTargetGlsl,
                                        kUniformFloats, &fragment_source, &error)) {
        std::fprintf(stderr, "[eshi/gl] %s\n", error.c_str());
        return NULL;
    }

    GlBackend* backend = new GlBackend();
    std::memset(&backend->gl, 0, sizeof(backend->gl));
    backend->width = width;
    backend->height = height;
    backend->program = 0;
    backend->fbo = 0;
    backend->fbo_texture = 0;
    backend->vbo = 0;
    backend->vao = 0;
    backend->loc_resolution = -1;
    backend->loc_time = -1;
    backend->loc_uniforms = -1;
    backend->scratch.resize((size_t)width * (size_t)height * 4);

    if (!load_functions(&backend->gl, g_proc_loader)) {
        delete backend;
        return NULL;
    }
    const GlFunctions& gl = backend->gl;

    gl.GenVertexArrays(1, &backend->vao);
    gl.BindVertexArray(backend->vao);

    const GLfloat quad[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
    gl.GenBuffers(1, &backend->vbo);
    gl.BindBuffer(GL_ARRAY_BUFFER, backend->vbo);
    gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(quad), quad, GL_STATIC_DRAW);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*)0);

    static const char* kVertexSource =
        "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "void main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n";

    GLuint vertex = gl.CreateShader(GL_VERTEX_SHADER);
    gl.ShaderSource(vertex, 1, &kVertexSource, NULL);
    if (!compile_stage(gl, vertex, "vertex shader")) { delete backend; return NULL; }

    const char* fragment_ptr = fragment_source.c_str();
    GLuint fragment = gl.CreateShader(GL_FRAGMENT_SHADER);
    gl.ShaderSource(fragment, 1, &fragment_ptr, NULL);
    if (!compile_stage(gl, fragment, "fragment shader")) { delete backend; return NULL; }

    backend->program = gl.CreateProgram();
    gl.AttachShader(backend->program, vertex);
    gl.AttachShader(backend->program, fragment);
    gl.LinkProgram(backend->program);

    GLint linked = 0;
    gl.GetProgramiv(backend->program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[2048];
        gl.GetProgramInfoLog(backend->program, (GLsizei)sizeof(log), NULL, log);
        std::fprintf(stderr, "[eshi/gl] link failed:\n%s\n", log);
        delete backend;
        return NULL;
    }

    backend->loc_resolution = gl.GetUniformLocation(backend->program, "iResolution");
    backend->loc_time = gl.GetUniformLocation(backend->program, "iTime");
    backend->loc_uniforms = gl.GetUniformLocation(backend->program, "eshi_uniforms");

    /* Render target: an offscreen FBO we read back, matching the Ink contract. */
    gl.GenTextures(1, &backend->fbo_texture);
    gl.BindTexture(GL_TEXTURE_2D, backend->fbo_texture);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gl.GenFramebuffers(1, &backend->fbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER, backend->fbo);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            backend->fbo_texture, 0);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[eshi/gl] framebuffer incomplete\n");
        delete backend;
        return NULL;
    }
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);

    std::printf("[eshi/gl] OpenGL 3.3 backend ready\n");
    return reinterpret_cast<EshiBackend*>(backend);
}

void gl_destroy(EshiBackend* handle) {
    GlBackend* backend = reinterpret_cast<GlBackend*>(handle);
    if (!backend) return;
    if (backend->program && backend->gl.DeleteProgram) {
        backend->gl.DeleteProgram(backend->program);
    }
    delete backend;
}

EshiResult gl_render(EshiBackend* handle,
                     uint8_t* pixels, int32_t stride, float time,
                     EshiShaderFn /*cpu_shader*/,
                     const void* uniforms, size_t uniform_size) {
    GlBackend* backend = reinterpret_cast<GlBackend*>(handle);
    if (!backend) return ESHI_ERR_INVALID;

    const GlFunctions& gl = backend->gl;
    const int32_t width = backend->width;
    const int32_t height = backend->height;

    gl.BindFramebuffer(GL_FRAMEBUFFER, backend->fbo);
    gl.Viewport(0, 0, width, height);
    gl.UseProgram(backend->program);

    if (backend->loc_resolution >= 0) {
        gl.Uniform2f(backend->loc_resolution, (GLfloat)width, (GLfloat)height);
    }
    if (backend->loc_time >= 0) {
        gl.Uniform1f(backend->loc_time, time);
    }
    if (backend->loc_uniforms >= 0 && uniforms && uniform_size >= sizeof(float)) {
        gl.Uniform1fv(backend->loc_uniforms,
                      (GLsizei)(uniform_size / sizeof(float)),
                      static_cast<const GLfloat*>(uniforms));
    }

    gl.BindVertexArray(backend->vao);
    gl.DrawArrays(GL_TRIANGLE_FAN, 0, 4);

    /*
     * glReadPixels returns rows bottom-up: offset 0 is the row where
     * gl_FragCoord.y is smallest. The engine's framebuffer is top-down — Ink
     * writes row 0 from the largest fragCoord.y — so the readback must be
     * flipped, not copied straight through.
     *
     * The original renderer_gl.h did copy it straight through (line 323), which
     * left every GPU render vertically mirrored against the CPU one. It went
     * unnoticed because most of the gallery is vertically symmetric enough to
     * hide it; the Ink-vs-GPU pixel diff is what surfaced it.
     */
    gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    const size_t packed_stride = (size_t)width * 4;
    gl.ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, backend->scratch.data());
    for (int32_t y = 0; y < height; ++y) {
        std::memcpy(pixels + (size_t)y * (size_t)stride,
                    backend->scratch.data() + (size_t)(height - 1 - y) * packed_stride,
                    packed_stride);
    }

    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    return ESHI_OK;
}

const EshiBackendVTable kGlVTable = {
    "gl",
    gl_available,
    gl_create,
    gl_destroy,
    gl_render,
};

} /* namespace */

extern "C" const EshiBackendVTable* eshi__backend_gl(void) { return &kGlVTable; }

extern "C" void eshi_gl_set_proc_loader(EshiGlProcLoader loader) {
    g_proc_loader = loader;
}

extern "C" EshiGlProcLoader eshi__gl_proc_loader(void) { return g_proc_loader; }
