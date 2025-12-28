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
#include "glsl_core.h"
#include "renderer_cpu.h" // Include our new worker
#include <string>
#include <iostream>

// Helper to extract filename
std::string get_filename(std::string path) {
    const size_t last_slash_idx = path.find_last_of("\\/");
    if (std::string::npos != last_slash_idx) path.erase(0, last_slash_idx + 1);
    const size_t period_idx = path.rfind('.');
    if (std::string::npos != period_idx) path.erase(period_idx);
    return path;
}

int main(int argc, char** argv) {
    // --- CONFIGURATION ---
    const int W = 16 * 60;   // 960 px
    const int H = 9 * 60;    // 540 px
    const int FRAMES = 240;
    const int FPS = 60;

    // --- SETUP OUTPUT NAME ---
    std::string output_name = "output.mp4";
    if (argc > 0) {
        std::string bin_name = get_filename(argv[0]);
        if (!bin_name.empty()) output_name = bin_name + ".mp4";
    }

    // --- INITIALIZE SYSTEMS ---
    printf("Initializing Engine...\n");
    printf("Output: %s [%dx%d @ %d FPS]\n", output_name.c_str(), W, H, FPS);

    SimpleEncoder video(output_name.c_str(), W, H, FPS);
    CpuRenderer renderer(W, H); // Future: GpuRenderer renderer(W, H);

    // --- RENDER LOOP ---
    printf("Rendering %d frames...\n", FRAMES);
    
    for (int i = 0; i < FRAMES; ++i) {
        float time = (float)i / (float)FPS; // Correct time calculation based on FPS

        // 1. Get writable buffer from video encoder
        int stride;
        uint8_t* pixels = video.get_pixel_buffer(stride);

        // 2. Ask Renderer to fill it
        renderer.renderFrame(pixels, stride, time);

        // 3. Send back to video encoder
        video.submit_frame();
    }

    return 0;
}
