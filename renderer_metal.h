#pragma once
#include <stdint.h>
#include <string>

class MetalRenderer {
public:
    MetalRenderer(int w, int h, const char* shaderPath, float* texData = nullptr, int texW = 0, int texH = 0);
    ~MetalRenderer();
    
    // The core interface Eshi expects
    void renderFrame(uint8_t* pixelBuffer, int stride, float time);

private:
    int width, height;
    void* device = nullptr;       // id<MTLDevice>
    void* commandQueue = nullptr; // id<MTLCommandQueue>
    void* pipelineState = nullptr;// id<MTLComputePipelineState> or RenderPipelineState
    void* texture = nullptr;      // id<MTLTexture> for user input
    void* outputTexture = nullptr;// id<MTLTexture> to render into
    void* outputBuffer = nullptr; // id<MTLBuffer> to copy pixels back to CPU
};
