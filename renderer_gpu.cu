#include <cuda_runtime.h>
#include <stdio.h>
#include "glsl_core.h"
#include "renderer_gpu.h" 

using namespace glsl;

__device__ Sampler2D iChannel0;

namespace gpu {
	#ifndef SHADER_PATH
	    #include "shader.cpp"
	#else
	    #include SHADER_PATH
	#endif
}

void uploadTextureToGPU(int w, int h, vec4* host_data) {
    vec4* d_data;
    size_t size = w * h * sizeof(vec4);
    cudaMalloc(&d_data, size);
    cudaMemcpy(d_data, host_data, size, cudaMemcpyHostToDevice);
    
    Sampler2D temp;
    temp.w = w;
    temp.h = h;
    temp.data = d_data;
    
    cudaMemcpyToSymbol(iChannel0, &temp, sizeof(Sampler2D));
    printf("GPU: Texture uploaded (%dx%d)\n", w, h);
}

__global__ void renderKernel(uchar4* output, int width, int height, float time) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    vec2 iResolution((float)width, (float)height);
    vec2 fragCoord((float)x, (float)(height - 1 - y));
    vec4 color;

    // Call with 4 arguments
    gpu::mainImage(color, fragCoord, iResolution, time);

    float r = ::fmaxf(0.0f, ::fminf(color.x, 1.0f));
    float g = ::fmaxf(0.0f, ::fminf(color.y, 1.0f));
    float b = ::fmaxf(0.0f, ::fminf(color.z, 1.0f));

    int idx = y * width + x;
    output[idx] = make_uchar4((unsigned char)(r * 255.0f), (unsigned char)(g * 255.0f), (unsigned char)(b * 255.0f), 255);
}

uchar4* d_buffer = NULL; 
int g_width, g_height;

GpuRenderer::GpuRenderer(int w, int h) {
    g_width = w; g_height = h;
    printf("GPU Renderer: Initializing CUDA... ");
    cudaMalloc(&d_buffer, w * h * sizeof(uchar4));
    printf("Ready.\n");
}

GpuRenderer::~GpuRenderer() {
    if (d_buffer) cudaFree(d_buffer);
}

void GpuRenderer::renderFrame(uint8_t* pixelBuffer, int stride, float time) {
    dim3 blockSize(16, 16);
    dim3 gridSize((g_width + blockSize.x - 1) / blockSize.x, (g_height + blockSize.y - 1) / blockSize.y);

    renderKernel<<<gridSize, blockSize>>>(d_buffer, g_width, g_height, time);
    cudaDeviceSynchronize();
    
    cudaMemcpy(pixelBuffer, d_buffer, g_width * g_height * sizeof(uchar4), cudaMemcpyDeviceToHost);
}
