#pragma once
#include <stdint.h>

class GpuRenderer {
public:
    GpuRenderer(int w, int h);
    ~GpuRenderer();
    void renderFrame(uint8_t* pixelBuffer, int stride, float time);
};
