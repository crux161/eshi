#include "glsl_core.h"
#include "renderer_cpu.h" 

#include <string>
#include <iostream>

#ifdef USE_CUDA
    #include "renderer_gpu.h"
#endif

std::string get_filename(std::string path) {
    const size_t last_slash_idx = path.find_last_of("\\/");
    if (std::string::npos != last_slash_idx) path.erase(0, last_slash_idx + 1);
    const size_t period_idx = path.rfind('.');
    if (std::string::npos != period_idx) path.erase(period_idx);
    return path;
}

int main(int argc, char** argv) {
    const int W = 16 * 60;   // 960 px
    const int H = 9 * 60;    // 540 px
    const int FRAMES = 240;
    const int FPS = 60;

    std::string output_name = "output.mp4";
    if (argc > 0) {
        std::string bin_name = get_filename(argv[0]);
        if (!bin_name.empty()) output_name = bin_name + ".mp4";
    }

    bool use_gpu = false;
    for(int i=0; i<argc; i++){
	if(std::string(argv[i]) == "--gpu") { use_gpu = true; }
    }

    printf("Initializing Engine...\n");
    printf("Output: %s [%dx%d @ %d FPS]\n", output_name.c_str(), W, H, FPS);

    SimpleEncoder video(output_name.c_str(), W, H, FPS);
    
    if (use_gpu) {
        #ifdef USE_CUDA
            printf("--- STARTING GPU RENDER ---\n");
            GpuRenderer gpu(W, H);
            
            for (int i = 0; i < FRAMES; ++i) {
                float time = (float)i / (float)FPS;
                int stride;
                uint8_t* pixels = video.get_pixel_buffer(stride);
                gpu.renderFrame(pixels, stride, time);
                video.submit_frame();
            }
        #else
            printf("Error: GPU mode requested but engine was compiled without CUDA.\n");
            return 1;
        #endif
    } else {
        printf("--- STARTING CPU RENDER ---\n");
        CpuRenderer cpu(W, H);
        
        for (int i = 0; i < FRAMES; ++i) {
            float time = (float)i / (float)FPS;
            int stride;
            uint8_t* pixels = video.get_pixel_buffer(stride);
            cpu.renderFrame(pixels, stride, time);
            video.submit_frame();
        }
    }

    return 0;
}
/* Lambda clamping
	*
	* This version produces slightly off results,
	* with integer overflow noticable in macOS espcially.
	*
	* #pragma omp parallel for
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                
                vec4 color;
                vec2 fragCoord((float)x, (float)y);
                
                // CALL THE FUNCTION FROM shader.cpp
                mainImage(color, fragCoord, iResolution, time);

                // Write to buffer
                int idx = y * stride + x * 3;

		auto clamp_u8 = [](float v) {
		    if (v < 0.0f) v = 0.0f;
		    if (v > 1.0f) v = 1.0f;
		    return (uint8_t)(v * 255.0f);
		};
                pixels[idx + 0] = (uint8_t)(color.x * 255.0f);
                pixels[idx + 1] = (uint8_t)(color.y * 255.0f);
                pixels[idx + 2] = (uint8_t)(color.z * 255.0f);
            }
        }
	*
	*/


