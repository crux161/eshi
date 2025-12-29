#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "glsl_core.h"
#include "encoder.h"
#include "renderer_cpu.h" 
#include "display.h" 

#include <string>
#include <iostream>
#include <vector>

#ifdef USE_CUDA
    #include "renderer_gpu.h"
#endif

Sampler2D iChannel0 = {0, 0, nullptr};

std::string get_filename(std::string path) {
    const size_t last_slash_idx = path.find_last_of("\\/");
    if (std::string::npos != last_slash_idx) path.erase(0, last_slash_idx + 1);
    const size_t period_idx = path.rfind('.');
    if (std::string::npos != period_idx) path.erase(period_idx);
    return path;
}

int main(int argc, char** argv) {
    
    int W = 960; 
    int H = 540;
    const int FPS = 60;
    
    
    bool use_gpu = false;
    bool live_mode = false;
    std::string output_name = "output.mp4";

    
    if (argc > 0) {
        std::string bin_name = get_filename(argv[0]);
        if (!bin_name.empty()) output_name = bin_name + ".mp4";
    }

    
    for(int i=1; i<argc; i++){
        std::string arg = argv[i];
        
        if(arg == "--gpu")  use_gpu = true;
        if(arg == "--live") live_mode = true;
        
        
        if(arg == "--res") {
            if (i + 1 < argc) {
                std::string res_str = argv[++i]; 
                size_t x_pos = res_str.find('x');
                if (x_pos != std::string::npos) {
                    try {
                        W = std::stoi(res_str.substr(0, x_pos));
                        H = std::stoi(res_str.substr(x_pos + 1));
                    } catch (...) {
                        fprintf(stderr, "Invalid resolution format. Use WIDTHxHEIGHT (e.g. 1280x720)\n");
                    }
                }
            }
        }
    }

    
    
    const char* texPath = "texture.jpg"; 
    int tw, th, tn;
    float* texData = stbi_loadf(texPath, &tw, &th, &tn, 4); 
    
    if (texData) {
        printf("Texture loaded: %s [%dx%d]\n", texPath, tw, th);
        
        
        
        vec4* vecData = new vec4[tw * th];
        for (int i = 0; i < tw * th; i++) {
            vecData[i] = vec4(texData[i*4+0], texData[i*4+1], texData[i*4+2], texData[i*4+3]);
        }
        
        
        iChannel0.w = tw;
        iChannel0.h = th;
        iChannel0.data = vecData; 
        
        
        #ifdef USE_CUDA
        if (use_gpu) {
            uploadTextureToGPU(tw, th, vecData);
        }
        #endif
        
        stbi_image_free(texData);
    } else {
        printf("⚠️ No texture found at %s. iChannel0 will be empty.\n", texPath);
    }

    printf("Initializing Engine...\n");
    if (live_mode) printf("Mode: Live Window [%dx%d]\n", W, H);
    else           printf("Mode: File Render -> %s [%dx%d]\n", output_name.c_str(), W, H);

    
    CpuRenderer* cpu_renderer = nullptr;
    #ifdef USE_CUDA
    GpuRenderer* gpu_renderer = nullptr;
    
    
    if (use_gpu) gpu_renderer = new GpuRenderer(W, H);
    else 
    #endif
    cpu_renderer = new CpuRenderer(W, H);

    
    if (live_mode) {
        Display window(W, H, "Eshi Live Preview");
        float time = 0.0f;
        
        while (window.isOpen()) {
            int stride;
            uint8_t* pixels = window.get_pixel_buffer(stride);
            
            #ifdef USE_CUDA
            if (use_gpu && gpu_renderer) gpu_renderer->renderFrame(pixels, stride, time);
            else 
            #endif
            if (cpu_renderer) cpu_renderer->renderFrame(pixels, stride, time);
            
            window.submit_frame();
            time += 1.0f / 60.0f; 
        }

    } else {
        
        const int FRAMES = 240;
        SimpleEncoder video(output_name.c_str(), W, H, FPS);
        
        for (int i = 0; i < FRAMES; ++i) {
            float time = (float)i / (float)FPS;
            int stride;
            uint8_t* pixels = video.get_pixel_buffer(stride);

            #ifdef USE_CUDA
            if (use_gpu && gpu_renderer) gpu_renderer->renderFrame(pixels, stride, time);
            else 
            #endif
            if (cpu_renderer) cpu_renderer->renderFrame(pixels, stride, time);

            video.submit_frame();
        }
    }

    
    if (cpu_renderer) delete cpu_renderer;
    #ifdef USE_CUDA
    if (gpu_renderer) delete gpu_renderer;
    #endif

    return 0;
}
