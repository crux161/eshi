/**
 * @file ink.cpp
 * @brief Kantei Grade 1 (Ink) — CPU/OpenMP fullscreen shader backend.
 *
 * The tier below Filament's GLES 2.0 floor: no GPU, no driver, no context. It
 * is what lets Eshi run on an ESP32, a Playdate, or a headless build box, and
 * it is the reference oracle the GPU tiers are pixel-diffed against. It is
 * always available, which is why grade selection can never fail outright.
 *
 * Ink is the only backend that executes a host function pointer. The GPU tiers
 * cannot, which is the whole reason EshiMaterial carries both a compiled-in
 * entry point and a source path.
 */
#include <cstddef>
#include <cstdint>
#include <new>

#include "../eshi_internal.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

struct InkBackend {
    int32_t width;
    int32_t height;
};

int ink_available(void) { return 1; }

EshiBackend* ink_create(int32_t width, int32_t height,
                        const char* /*source_path*/, const char* /*package_path*/) {
    InkBackend* backend = new (std::nothrow) InkBackend();
    if (!backend) return NULL;
    backend->width = width;
    backend->height = height;
    return reinterpret_cast<EshiBackend*>(backend);
}

void ink_destroy(EshiBackend* handle) {
    delete reinterpret_cast<InkBackend*>(handle);
}

EshiResult ink_render(EshiBackend* handle,
                      uint8_t* pixels, int32_t stride, float time,
                      EshiShaderFn cpu_shader,
                      const void* uniforms, size_t /*uniform_size*/) {
    InkBackend* backend = reinterpret_cast<InkBackend*>(handle);
    if (!cpu_shader) return ESHI_ERR_INVALID;

    const int32_t width = backend->width;
    const int32_t height = backend->height;
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
            cpu_shader(rgba,
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

const EshiBackendVTable kInkVTable = {
    "ink",
    ink_available,
    ink_create,
    ink_destroy,
    ink_render,
    /* No 3D scene: assets are refused rather than half-supported. */
    NULL,
    NULL,
    NULL,
    NULL,
};

} /* namespace */

extern "C" const EshiBackendVTable* eshi__backend_ink(void) { return &kInkVTable; }
