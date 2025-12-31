#pragma once
#include <stdint.h>
#include "glsl_core.h"

void uploadTextureToGPU(int w, int h, glsl::vec4* host_data);

class GpuRenderer {
public:
    GpuRenderer(int w, int h);
    ~GpuRenderer();
    void renderFrame(uint8_t* pixelBuffer, int stride, float time);
};
