/**
 * @file render_ink.cpp
 * @brief Kantei Grade 1 (Ink) — CPU/OpenMP fullscreen shader backend.
 *
 * This is the tier below Filament's GLES 2.0 floor: no GPU, no driver, no
 * context. It is what lets Eshi run on an ESP32, a Playdate, or a headless
 * build box, and it is the reference oracle the GPU tiers get pixel-diffed
 * against. It is not scaffolding.
 *
 * The rendering model is deliberate: the material *is* a fullscreen shader,
 * and the ECS supplies its parameters through an opaque uniform block. The
 * engine never learns what is in that block, which is why adding a game no
 * longer forks the backend signature the way feature/pong-engine did.
 */
#include "eshi/eshi.h"
#include "eshi_internal.h"

#ifdef _OPENMP
#include <omp.h>
#endif

extern "C" EshiResult eshi_render(EshiWorld* w, uint8_t* pixels, int32_t stride, float time) {
    if (!w || !pixels || stride <= 0) return ESHI_ERR_INVALID;

    EshiShaderFn shader = 0;
    const void*  uniforms = 0;
    int32_t      width = 0, height = 0;
    eshi__frame_params(w, &shader, &uniforms, &width, &height);

    if (!shader) return ESHI_ERR_INVALID;
    if (stride < width * 4) return ESHI_ERR_INVALID;

    const float res_x = (float)width;
    const float res_y = (float)height;

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int32_t y = 0; y < height; ++y) {
        uint8_t* row = pixels + (ptrdiff_t)y * stride;
        for (int32_t x = 0; x < width; ++x) {
            float rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};

            /* Origin bottom-left, pixel centre — Shadertoy's convention. */
            shader(rgba,
                   (float)x + 0.5f,
                   (float)(height - 1 - y) + 0.5f,
                   res_x, res_y, time, uniforms);

            for (int c = 0; c < 4; ++c) {
                float v = rgba[c];
                if (!(v > 0.0f)) v = 0.0f; /* also catches NaN */
                if (v > 1.0f) v = 1.0f;
                row[x * 4 + c] = (uint8_t)(v * 255.0f + 0.5f);
            }
        }
    }

    return ESHI_OK;
}
