#include <cuda_runtime.h>
#include <stdio.h>

#include "glsl_core.h"
#include "renderer_gpu.h" 

namespace gpu {
	#ifndef SHADER_PATH
	    #include "shader.cpp"
	#else
	    #include SHADER_PATH
	#endif
}

__global__ void renderKernel(uchar3* output, int width, int height, float time) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    vec2 iResolution((float)width, (float)height);
    vec2 fragCoord((float)x, (float)y);
    vec4 color;

    gpu::mainImage(color, fragCoord, iResolution, time);

    float r = fmaxf(0.0f, fminf(color.x, 1.0f));
    float g = fmaxf(0.0f, fminf(color.y, 1.0f));
    float b = fmaxf(0.0f, fminf(color.z, 1.0f));

    int idx = y * width + x;
    output[idx] = make_uchar3((unsigned char)(r * 255.0f), (unsigned char)(g * 255.0f), (unsigned char)(b * 255.0f));
}

// Pointer to gpu memory
uchar3* d_buffer = NULL; 
int g_width, g_height;

GpuRenderer::GpuRenderer(int w, int h) {
    g_width = w; g_height = h;
    printf("GPU Renderer: Initializing CUDA... ");
    cudaMalloc(&d_buffer, w * h * sizeof(uchar3));
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

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
	fprintf(stderr, "GPU Kernel Error: %s\n", cudaGetErrorString(err));
    }

    cudaDeviceSynchronize();
    // Copy back to CPU buffer
    // Note: This assumes stride == width * 3 for simplicity. 
    // If FFmpeg gives padding, we might need a row-by-row copy, but usually it works.
    cudaMemcpy(pixelBuffer, d_buffer, g_width * g_height * sizeof(uchar3), cudaMemcpyDeviceToHost);
}
