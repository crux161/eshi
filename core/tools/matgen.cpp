/**
 * @file matgen.cpp
 * @brief Emit a Filament material definition from a shader source.
 *
 * Build-time only. It exists so that `examples/pong/pong.mat` does not: that
 * file was a hand transliteration of `examples/pong/pong.gpu.cpp` — same
 * distance functions, same constants, same score loop — and two copies of one
 * material agree right up until somebody edits one of them.
 *
 * This links the same transpiler the runtime uses for GLSL and MSL, so a
 * material and a shader cannot drift apart in their reading of the subset
 * either. `matc` turns the output into the `.filamat` the Brush tier loads.
 *
 * usage: eshi-matgen <shader-source> [-o out.mat] [--uniform-floats N]
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "../src/render/transpile.h"

namespace {

/*
 * The Filament backend's uniform block capacity. Declaring the full block
 * rather than a per-game count keeps the material independent of how many
 * floats a particular game happens to use; the runtime writes a prefix of it.
 */
const int kDefaultUniformFloats = 64;

int usage(const char* program) {
    std::fprintf(stderr,
                 "usage: %s <shader-source> [-o out.mat] [--uniform-floats N]\n",
                 program);
    return 2;
}

} /* namespace */

int main(int argc, char** argv) {
    std::string source_path;
    std::string output_path;
    int uniform_floats = kDefaultUniformFloats;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "-o" || argument == "--output") {
            if (++i >= argc) return usage(argv[0]);
            output_path = argv[i];
        } else if (argument == "--uniform-floats") {
            if (++i >= argc) return usage(argv[0]);
            uniform_floats = std::atoi(argv[i]);
            if (uniform_floats < 0) return usage(argv[0]);
        } else if (!argument.empty() && argument[0] == '-') {
            return usage(argv[0]);
        } else if (source_path.empty()) {
            source_path = argument;
        } else {
            return usage(argv[0]);
        }
    }
    if (source_path.empty()) return usage(argv[0]);

    std::string material, error;
    if (!eshi::transpile::build_program(source_path, eshi::transpile::kTargetMat,
                                        uniform_floats, &material, &error)) {
        std::fprintf(stderr, "eshi-matgen: %s\n", error.c_str());
        return 1;
    }

    if (output_path.empty()) {
        std::fwrite(material.data(), 1, material.size(), stdout);
        return 0;
    }

    std::ofstream out(output_path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) {
        std::fprintf(stderr, "eshi-matgen: cannot write %s\n", output_path.c_str());
        return 1;
    }
    out.write(material.data(), (std::streamsize)material.size());
    if (!out) {
        std::fprintf(stderr, "eshi-matgen: failed while writing %s\n", output_path.c_str());
        return 1;
    }
    return 0;
}
