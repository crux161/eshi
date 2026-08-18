# The Larimar shader subset

> Status: descriptive, not aspirational. Everything here is what
> `core/src/render/transpile.cpp` already does and what the 20 programs in
> `examples/` already obey. It answers open question 5 in
> [`ARCHITECTURE.md`](ARCHITECTURE.md) §9 and is the contract PLAN Step 6b's S2L
> frontend must lower into.

A Larimar material is written once, as C++. That single file is:

- **compiled** by the C++ compiler for the Ink tier, where it runs on the CPU;
- **transpiled** at runtime to GLSL 3.30 for Paper and to MSL for Brush/Metal;
- **emitted** at build time as a Filament `.mat` for Brush/Filament, which
  `matc` compiles into a `.filamat`.

The subset is the intersection of what those four consumers accept. It is
enforced by a line-oriented textual converter, not a parser, and this document
says so plainly: the rules below are what that converter can see.

## 1. The entry point

```cpp
SHADER_CTX void mainImage(vec4 &fragColor, vec2 fragCoord,
                          vec2 iResolution, float iTime);
```

`fragCoord` is in pixels, origin bottom-left, matching `gl_FragCoord`. Every
target reconstructs that convention rather than exposing its own: the Metal
kernel flips the thread's y, and the Filament material rebuilds it from the
device-domain clip position. One coordinate system is what makes the tiers
comparable pixel for pixel.

`fragColor` is written, never read. Other reference parameters are `inout` and
may be read before assignment — `mario.cpp`'s sprite helpers composite onto what
is already there — so the converter never promotes them to `out`.

## 2. Types and calls

- `float`, `int`, `bool`, `vec2/3/4`, `ivec*`, `bvec*`, `mat2/3/4`.
- The `glsl::`/`sumi::` namespace qualifiers are stripped; write them or don't.
- Swizzles may be written as calls — `p.xy()` — which the converter rewrites to
  members. That spelling exists because the CPU side implements them as methods.
- The libc float intrinsics (`sinf`, `powf`, `fabsf`, …) map to the shading
  languages' unsuffixed names through a generated preamble.
- Float literals keep their `f` suffix. All four targets accept it.

## 3. What each target adds

| Rule | Ink | GLSL | MSL | `.mat` |
|---|---|---|---|---|
| `#include`, `#pragma`, `using namespace`, `extern` | kept | stripped | stripped | stripped |
| C-style casts `(float)x` | ok | rewritten to `float(x)` | ok | rewritten |
| Direct init `vec2 c(0,1);` | ok | rewritten to `= vec2(0,1)` | ok | rewritten |
| `(void)x;` | ok | removed | ok | removed |
| `noise1`–`noise4` | ok | renamed `eshi_noise*` (reserved) | ok | renamed |
| `PI`, `HALF_PI` | ok | ok | ok | renamed `eshi_PI` (Filament defines them) |
| `eshi_uniforms[i]` | typed struct | global array | kernel argument | `materialParams.eshiUniforms` |
| `iChannel0` | sampled | `sampler2D` | `texture2d` | **not available** |

An `extern` declaration takes its whole body with it — `mainSound` is host code
and has no business in a shader.

Uniforms are read only inside `mainImage`: MSL binds the block as a kernel
argument threaded through that signature, so a helper function cannot see it.

## 4. What the `.mat` target cannot express

Filament owns the vertex stage, the varyings, the uniform layout, and the
lighting; a fragment block fills in `MaterialInputs` under a declared shading
model. Larimar's fullscreen materials map onto `shadingModel : unlit` with
`vertexDomain : device`, which is a clean fit — 18 of the 20 corpus programs
compile through it unchanged. Two do not, and both refuse at generation time
with a diagnostic naming the cause rather than emitting something plausible:

**Texture sampling.** `warp.cpp` reads `iChannel0`. The generated material
declares no sampler and the Filament backend binds no texture, so there is
nothing to sample. Adding a sampler parameter is possible and deliberately not
in RC0; the shader remains available on Paper and Ink.

**Braces that only balance after preprocessing.** `rainforest.cpp` opens its
march loop inside `#ifdef LOWQUALITY` and again inside the `#else`. Every
compiler in the chain handles that correctly, but `matc` splits a `.mat` into
blocks by counting braces *before* preprocessing, so the fragment block runs
past its own closing brace and matc reports an unexpected character on a line
that looks fine. The rule for materials is therefore stricter than for shaders:
a conditional may not open a brace it does not close.

`scripts/check_materials.sh` holds that list. A shader may only fail to emit if
it is named there, and a shader that starts emitting must be removed from it —
the set cannot grow or shrink without someone editing the file.

## 5. What the subset is not

It is not a language, and `transpile.cpp` is not a compiler. There is no type
checker, no scope analysis, and no error for a construct that happens to survive
the substitutions and mean something different afterwards. What keeps that
honest is the conformance oracle rather than the converter:
`scripts/check_tier_conformance.sh` renders the same source on every available
tier and requires the frames to agree to within one least-significant bit.

PLAN Step 6b brings S2L in as a frontend that emits this subset. The subset is
what carries forward; the frontend is a nicer way to write it.
