/**
 * @file eshi_internal.h
 * @brief Cross-translation-unit surface inside the core. Never installed.
 *
 * Backends need a handful of facts about the world without depending on its
 * layout, so the world exposes them through this narrow accessor rather than
 * publishing its struct. That keeps render backends swappable — the Filament
 * backend in Phase 1 links against exactly this and nothing more.
 */
#ifndef ESHI_INTERNAL_H
#define ESHI_INTERNAL_H

#include "eshi/eshi.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Everything a render backend needs for one frame. Any out-param may be NULL. */
void eshi__frame_params(EshiWorld* w,
                        EshiShaderFn* out_shader,
                        const void**  out_uniforms,
                        int32_t*      out_width,
                        int32_t*      out_height);

#ifdef __cplusplus
}
#endif

#endif /* ESHI_INTERNAL_H */
