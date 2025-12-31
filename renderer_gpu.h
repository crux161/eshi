#pragma once
#include <cstdint>
#include "glsl_core.h"

class GpuRenderer {
public:
    GpuRenderer(int w, int h);
    ~GpuRenderer();
    void renderFrame(uint8_t* pixelBuffer, int stride, float time, glsl::GameData* gameData);

private:
    // These were missing!
    int width;
    int height;
};

// Global helper
void uploadTextureToGPU(int w, int h, void* data);
