#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
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

    std::string readFile(std::string path) {
        std::string cleanPath = resolvePath(path);
        if (cleanPath.empty()) {
            printf("[GL ERROR] File not found: %s\n", path.c_str());
            exit(1);
        }

        std::ifstream f(cleanPath);
        std::string content, line;
        while(std::getline(f, line)) {
            
            if(line.find("#include") != std::string::npos) continue;
            if(line.find("#pragma") != std::string::npos) continue;
            if(line.find("using namespace") != std::string::npos) continue;
            if(line.find("extern") != std::string::npos) continue; 
            
            
            line = replaceAll(line, "inline ", "");
            line = replaceAll(line, "vec4 &fragColor", "out vec4 fragColor");
            line = replaceAll(line, "vec4 &", "out vec4 ");
            
            
            line = replaceAll(line, "if (iChannel0.data == nullptr)", "if (false)");

            
            line = replaceAll(line, ".xyyx()", ".xyyx");
            line = replaceAll(line, ".xyz()", ".xyz");
            line = replaceAll(line, ".xy()", ".xy");
            line = replaceAll(line, ".yx()", ".yx");
            line = replaceAll(line, ".x()", ".x");
            line = replaceAll(line, ".y()", ".y");
            line = replaceAll(line, ".z()", ".z");
            line = replaceAll(line, ".w()", ".w");

            content += line + "\n";
        }
        return content;
    }

    void checkShader(GLuint shader) {
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "Shader Error:\n" << infoLog << std::endl;
        }
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

        std::string userCode = readFile(shaderPath);
        std::string fsSrc = "#version 330 core\n"
                            "out vec4 FragColor;\n"
                            "uniform vec2 iResolution;\n"
                            "uniform float iTime;\n"
                            "uniform sampler2D iChannel0;\n"
                            "\n"
                            "// --- Compatibility Wrapper ---\n"
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
        checkShader(fs);

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glUseProgram(program);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &fbo_texture);
        glBindTexture(GL_TEXTURE_2D, fbo_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("[GL] FBO Error\n");
        else printf("[GL] Ready.\n");
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

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixelBuffer);
    }
};
