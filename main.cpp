#include "glsl_core.h"

#ifndef LINK_SHADER
	#include "shader.cpp"
#else
	void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime);
#endif

#include <string>
#include <omp.h>

std::string get_filename(std::string path) {
    const size_t last_slash_idx = path.find_last_of("\\/");
    if (std::string::npos != last_slash_idx) {
        path.erase(0, last_slash_idx + 1);
    }

    const size_t period_idx = path.rfind('.');
    if (std::string::npos != period_idx) {
        path.erase(period_idx);
    }
    return path;
}

int main(int argc, char **argv) {

    // todo: replace, parse from somewhere else
    const int W = 16 * 60;
    const int H = 9 * 60;
    const int FRAMES = 240;
    const int FPS = 60;

    std::string output_file = "output.mp4"; // default
    
    if (argc > 0) {
	std::string bin_name = get_filename(argv[0]);
	if(!bin_name.empty()){
		output_file = bin_name + ".mp4";
	}
    }

    printf("Initializing Encoder: %s\n", output_file.c_str());

    SimpleEncoder video(output_file.c_str(), W, H, FPS);

    #ifdef _OPENMP
        printf("Using OpenMP with %d cores.\n", omp_get_max_threads());
    #endif

    for (int i = 0; i < FRAMES; ++i) {
        float time = (float)i / (float)FRAMES; // 0.0 to 1.0

        int stride;
        uint8_t* pixels = video.get_pixel_buffer(stride);
        vec2 iResolution((float)W, (float)H);

        #pragma omp parallel for
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                
                vec4 color;
                vec2 fragCoord((float)x, (float)y);
                
                // shader.cpp
                mainImage(color, fragCoord, iResolution, time);

                int idx = y * stride + x * 3;
                pixels[idx + 0] = (uint8_t)(color.x * 255.0f);
                pixels[idx + 1] = (uint8_t)(color.y * 255.0f);
                pixels[idx + 2] = (uint8_t)(color.z * 255.0f);
            }
        }
        video.submit_frame();
    }
    return 0;
}
