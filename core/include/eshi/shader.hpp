/**
 * @file shader.hpp
 * @brief C++ authoring layer over the C material ABI.
 *
 * The engine boundary is C, but nobody wants to write art against a
 * `float*`. This header lets a shader keep the Shadertoy-shaped signature the
 * gallery already uses, with libsumi vector types, and adapts it to
 * EshiShaderFn at compile time — no indirection beyond the one call the
 * backend already makes.
 *
 * The subset of C++ legal inside a shader body is the same subset the GPU
 * backends accept today (see renderer_gl.h / renderer_metal.mm). That subset
 * is the de-facto S2L and is what Phase 4 hands to matc; until it is written
 * down, treat the existing examples/ corpus as its definition.
 */
#ifndef ESHI_SHADER_HPP
#define ESHI_SHADER_HPP

#include <sumi/sumi.h>

#include "eshi.h"

namespace eshi {

/**
 * Adapts a typed `mainImage`-style function to the C material ABI.
 *
 * Usage:
 * @code
 *   struct MyUniforms { sumi::vec2 ball; };
 *   void mainImage(sumi::vec4& c, sumi::vec2 fragCoord,
 *                  sumi::vec2 iResolution, float iTime, const MyUniforms& u);
 *
 *   eshi_material_set(world,
 *                     eshi::shader<MyUniforms, mainImage>(),
 *                     &uniforms, sizeof(uniforms));
 * @endcode
 */
template <typename Uniforms,
          void (*MainImage)(sumi::vec4&, sumi::vec2, sumi::vec2, float, const Uniforms&)>
EshiShaderFn shader() {
    struct Adapter {
        static void call(float* out_rgba,
                         float frag_x, float frag_y,
                         float res_x, float res_y,
                         float time,
                         const void* uniforms) {
            sumi::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);
            MainImage(color,
                      sumi::vec2(frag_x, frag_y),
                      sumi::vec2(res_x, res_y),
                      time,
                      *static_cast<const Uniforms*>(uniforms));
            out_rgba[0] = color.x;
            out_rgba[1] = color.y;
            out_rgba[2] = color.z;
            out_rgba[3] = color.w;
        }
    };
    return &Adapter::call;
}

/** Same adaptation for a shader that takes no uniform block. */
template <void (*MainImage)(sumi::vec4&, sumi::vec2, sumi::vec2, float)>
EshiShaderFn shader_no_uniforms() {
    struct Adapter {
        static void call(float* out_rgba,
                         float frag_x, float frag_y,
                         float res_x, float res_y,
                         float time,
                         const void* /*uniforms*/) {
            sumi::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);
            MainImage(color,
                      sumi::vec2(frag_x, frag_y),
                      sumi::vec2(res_x, res_y),
                      time);
            out_rgba[0] = color.x;
            out_rgba[1] = color.y;
            out_rgba[2] = color.z;
            out_rgba[3] = color.w;
        }
    };
    return &Adapter::call;
}

} /* namespace eshi */

#endif /* ESHI_SHADER_HPP */
