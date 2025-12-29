#pragma once
#include "glsl_core.h"
#include <omp.h>
#include <algorithm> 


#ifndef LINK_SHADER
    #include "shader.cpp"
#else
    void mainImage(vec4 &fragColor, vec2 fragCoord, vec2 iResolution, float iTime);
#endif

class CpuRenderer {
    int width, height;

public:
    CpuRenderer(int w, int h) : width(w), height(h) {
        #ifdef _OPENMP
            printf("CPU Renderer: OpenMP Active (%d threads)\n", omp_get_max_threads());
        #else
            printf("CPU Renderer: Single Threaded\n");
        #endif
    }

    
    void renderFrame(uint8_t* pixelBuffer, int stride, float time) {
        vec2 iResolution((float)width, (float)height);

        #pragma omp parallel for
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                
                vec4 color;
                vec2 fragCoord((float)x, (float)y);
                mainImage(color, fragCoord, iResolution, time);

                
                
                float r = fmaxf(0.0f, fminf(color.x, 1.0f));
                float g = fmaxf(0.0f, fminf(color.y, 1.0f));
                float b = fmaxf(0.0f, fminf(color.z, 1.0f));

                
                int idx = y * stride + x * 3;
                pixelBuffer[idx + 0] = (uint8_t)(r * 255.0f);
                pixelBuffer[idx + 1] = (uint8_t)(g * 255.0f);
                pixelBuffer[idx + 2] = (uint8_t)(b * 255.0f);
            }
        }
    }
};
