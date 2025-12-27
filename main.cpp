#include "shader.cpp" 
#include <omp.h>

int main() {

    // todo: parse this data from somewhere 
    const int W = 16 * 60;
    const int H = 9 * 60;
    const int FRAMES = 240;
    const int FPS = 60;

    SimpleEncoder video("output.mp4", W, H, FPS);

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
