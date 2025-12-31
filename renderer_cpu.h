#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <omp.h>
#include "glsl_core.h"

// Forward declaration
void mainImage(glsl::vec4& fragColor, glsl::vec2 fragCoord, glsl::vec2 iResolution, float iTime, glsl::GameData game);

class CpuRenderer {
    int width;
    int height;

public:
    CpuRenderer(int w, int h) : width(w), height(h) {}

    void renderFrame(uint8_t* pixelBuffer, int stride, float time, glsl::GameData* gameData) {
        glsl::vec2 iResolution((float)width, (float)height);
        glsl::GameData currentGame = *gameData;

        // Auto-detect Bytes Per Pixel (BPP)
        // If stride is ~width*4, use 4 bytes. If ~width*3, use 3 bytes.
        int bpp = stride / width; 
        
        // Safety check: Default to 4 if calculation fails or padding throws it off significantly
        if (bpp != 3 && bpp != 4) bpp = 4;

        #pragma omp parallel for collapse(2)
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // GLSL coordinates (Y is up)
                glsl::vec2 fragCoord((float)x, (float)(height - 1 - y));
                
                glsl::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);

                // Run Shader
                mainImage(color, fragCoord, iResolution, time, currentGame);

                // Convert to 0-255
                int r = (int)(glsl::clamp(color.x, 0.0f, 1.0f) * 255.0f);
                int g = (int)(glsl::clamp(color.y, 0.0f, 1.0f) * 255.0f);
                int b = (int)(glsl::clamp(color.z, 0.0f, 1.0f) * 255.0f);

                // Calculate offset based on detected BPP
                int offset = y * stride + x * bpp;

                if (bpp == 4) {
                    // Standard RGBA (32-bit)
                    pixelBuffer[offset + 0] = (uint8_t)r;
                    pixelBuffer[offset + 1] = (uint8_t)g;
                    pixelBuffer[offset + 2] = (uint8_t)b;
                    pixelBuffer[offset + 3] = 255;
                } else {
                    // RGB (24-bit) - Fixes the glitch if Display uses this format
                    pixelBuffer[offset + 0] = (uint8_t)r;
                    pixelBuffer[offset + 1] = (uint8_t)g;
                    pixelBuffer[offset + 2] = (uint8_t)b;
                }
            }
        }
    }
};
