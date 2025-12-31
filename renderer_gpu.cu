#include "renderer_gpu.h"
#include <iostream>
#include <cuda_runtime.h>
#include "glsl_core.h"

// Define block sizes
#define BLOCK_W 16
#define BLOCK_H 16

namespace gpu {
    using namespace glsl; 
    #include SHADER_PATH
}

// --- UPDATED KERNEL: Handles Variable Bytes-Per-Pixel (BPP) ---
__global__ void renderKernel(uint8_t* pixelBuffer, int width, int height, int stride, int bpp, float time, glsl::GameData gameData) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    // Normalize coordinates
    glsl::vec2 iResolution((float)width, (float)height);
    glsl::vec2 fragCoord((float)x, (float)(height - 1 - y));

    glsl::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);

    // Call Shader
    gpu::mainImage(color, fragCoord, iResolution, time, gameData);

    // Color conversion
    int r = (int)(glsl::clamp(color.x, 0.0f, 1.0f) * 255.0f);
    int g = (int)(glsl::clamp(color.y, 0.0f, 1.0f) * 255.0f);
    int b = (int)(glsl::clamp(color.z, 0.0f, 1.0f) * 255.0f);

    // --- FIXED: Calculate offset using detected BPP ---
    int offset = y * stride + x * bpp;

    if (bpp == 4) {
        // 32-bit Mode (RGBA)
        pixelBuffer[offset + 0] = r;
        pixelBuffer[offset + 1] = g;
        pixelBuffer[offset + 2] = b;
        pixelBuffer[offset + 3] = 255; 
    } else {
        // 24-bit Mode (RGB)
        pixelBuffer[offset + 0] = r;
        pixelBuffer[offset + 1] = g;
        pixelBuffer[offset + 2] = b;
    }
}

GpuRenderer::GpuRenderer(int w, int h) : width(w), height(h) {}
GpuRenderer::~GpuRenderer() {}

void GpuRenderer::renderFrame(uint8_t* pixelBuffer, int stride, float time, glsl::GameData* gameData) {
    uint8_t* d_pixels;
    
    // Auto-detect BPP (Bytes Per Pixel)
    // If stride is 4x width, it's RGBA. If 3x, it's RGB.
    int bpp = stride / width;
    if (bpp != 3 && bpp != 4) bpp = 4; // Safety fallback

    size_t size = stride * height; 
    
    cudaMalloc(&d_pixels, size);

    dim3 block(BLOCK_W, BLOCK_H);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);

    glsl::GameData data = *gameData;

    // Pass BPP to Kernel
    renderKernel<<<grid, block>>>(d_pixels, width, height, stride, bpp, time, data);
    
    cudaDeviceSynchronize();
    cudaMemcpy(pixelBuffer, d_pixels, size, cudaMemcpyDeviceToHost);
    
    cudaFree(d_pixels);
}

void uploadTextureToGPU(int w, int h, void* data) {
    // Placeholder
    (void)w; (void)h; (void)data;
}
