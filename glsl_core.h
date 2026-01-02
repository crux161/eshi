#pragma once

// libsumi globally defines "namespace glsl = sumi;" at the end of its header.
#include <sumi/sumi.h>

// Compatibility macro for shader context (CUDA/Host)
#ifndef SHADER_CTX
    #define SHADER_CTX SUMI_CTX
#endif

// Ensure M_PI is available if not defined
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

// Forward declaration for procedural audio
// Returns stereo audio sample (vec2) for a given sample index and time
// Defined in specific example files (e.g. tunnelwisp.cpp), or defaults to silence.
extern "C" sumi::vec2 mainSound(int samp, float time);
