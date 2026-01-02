#pragma once
#include <stdint.h>
#include <sumi/sumi.h>

void uploadTextureToGPU(int w, int h, sumi::vec4* host_data);

class GpuRenderer {
public:
    GpuRenderer(int w, int h);
    ~GpuRenderer();
    void renderFrame(uint8_t* pixelBuffer, int stride, float time);
};
