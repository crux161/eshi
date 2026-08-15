/**
 * @file registry.cpp
 * @brief Maps Kantei grades to render backends.
 *
 * The only file that knows which backends exist. Adding the Filament backend in
 * Phase 1 means adding a case here and a vtable — the world, the hosts, and
 * every game stay untouched, which is the property the grade abstraction was
 * built for.
 *
 * Backends absent from a build get a stub that reports unavailable, so a binary
 * compiled without Metal still answers eshi_grade_available() honestly rather
 * than failing to link.
 */
#include <cstdio>

#include "../eshi_internal.h"

namespace {

EshiBackend* stub_create(int32_t, int32_t, const char*) { return NULL; }
void         stub_destroy(EshiBackend*) {}
int          stub_available(void) { return 0; }
EshiResult   stub_render(EshiBackend*, uint8_t*, int32_t, float,
                         EshiShaderFn, const void*, size_t) {
    return ESHI_ERR_UNSUPPORTED;
}

const EshiBackendVTable kStubVTable = {
    "unavailable", stub_available, stub_create, stub_destroy, stub_render,
};

} /* namespace */

#ifndef ESHI_HAVE_METAL
extern "C" const EshiBackendVTable* eshi__backend_metal(void) { return &kStubVTable; }
#endif

#ifndef ESHI_HAVE_GL
extern "C" const EshiBackendVTable* eshi__backend_gl(void) { return &kStubVTable; }
extern "C" void eshi_gl_set_proc_loader(EshiGlProcLoader) {}
extern "C" EshiGlProcLoader eshi__gl_proc_loader(void) { return NULL; }
#endif

extern "C" const EshiBackendVTable* eshi__backend_for_grade(EshiGrade grade) {
    switch (grade) {
        case ESHI_GRADE_INK:   return eshi__backend_ink();
        case ESHI_GRADE_PAPER: return eshi__backend_gl();
        case ESHI_GRADE_BRUSH: return eshi__backend_metal();
        /*
         * Gold is specialised hardware — ray tracing cores, neural accelerators.
         * Nothing implements it yet, and CUDA (renderer_gpu.cu) is the obvious
         * first candidate. Reporting unavailable is more useful than silently
         * handing back Brush.
         */
        case ESHI_GRADE_GOLD:  return &kStubVTable;
    }
    return &kStubVTable;
}

extern "C" int eshi_grade_available(EshiGrade grade) {
    const EshiBackendVTable* table = eshi__backend_for_grade(grade);
    return (table && table->available) ? table->available() : 0;
}

extern "C" EshiGrade eshi_grade_best(void) {
    if (eshi_grade_available(ESHI_GRADE_GOLD))  return ESHI_GRADE_GOLD;
    if (eshi_grade_available(ESHI_GRADE_BRUSH)) return ESHI_GRADE_BRUSH;
    if (eshi_grade_available(ESHI_GRADE_PAPER)) return ESHI_GRADE_PAPER;
    return ESHI_GRADE_INK;
}
