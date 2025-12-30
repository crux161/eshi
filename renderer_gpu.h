#pragma once
#include <stdint.h>

#include "glsl_core.h"

using namespace glsl;

class GpuRenderer {
public:
    GpuRenderer(int w, int h);
    ~GpuRenderer();
    void renderFrame(uint8_t* pixelBuffer, int stride, float time);
};


void uploadTextureToGPU(int w, int h, vec4* host_data);
