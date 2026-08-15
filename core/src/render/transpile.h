/**
 * @file transpile.h
 * @brief The C++ shader subset -> GLSL / MSL.
 *
 * This is the de-facto S2L, extracted from the two near-identical copies that
 * lived inside renderer_gl.h and renderer_metal.mm. It is line-oriented textual
 * substitution, and it is honest about that: it is not a parser and it does not
 * validate. What it defines is a *subset* of C++ that is simultaneously valid
 * CPU code and mechanically convertible to shader source — which is precisely
 * the property "write once, run on every tier" depends on.
 *
 * The 20 programs in examples/ are its conformance corpus. Phase 4 replaces the
 * emitters with .mat output for matc; the subset itself is what carries forward,
 * so consolidating it here is the prerequisite for writing it down as a spec.
 */
#ifndef ESHI_TRANSPILE_H
#define ESHI_TRANSPILE_H

#include <string>

namespace eshi {
namespace transpile {

enum Target {
    kTargetGlsl, /**< GLSL 3.30 core, fragment stage. */
    kTargetMsl   /**< Metal Shading Language, compute kernel. */
};

/**
 * Reads a shader source file and emits a complete, compilable program.
 *
 * @param path           Source path. Resolved against ., .., and ../.. so a
 *                       binary run from a build directory still finds it.
 * @param target         Which language to emit.
 * @param uniform_floats Size of the game's uniform block in floats. Zero omits
 *                       the `eshi_uniforms` binding entirely.
 * @param out_error      Set to a human-readable reason when this returns false.
 * @return               false if the source could not be read.
 */
bool build_program(const std::string& path,
                   Target target,
                   int uniform_floats,
                   std::string* out_source,
                   std::string* out_error);

} /* namespace transpile */
} /* namespace eshi */

#endif /* ESHI_TRANSPILE_H */
