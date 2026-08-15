# Larimar — Engineering Architecture

> Status: design, pre-implementation. Branch `feat/larimar`.
> Companion to [PROPOSAL.md](PROPOSAL.md), which records the original vision
> unedited. This document is the continuation of that proposal with the Gyosho
> monorepo (S2L / SumiC / Hanga) factored in, and it states plainly where the
> original phases need to change.

---

## 0. What changed

The original proposal was written as though SumiC were an existing asset to be
pointed at Filament. Having read `resources/gyosho`, that framing is backwards.
SumiC and S2L are not a shader-emitting utility waiting for a consumer — they are
**a language project whose entire backend strategy competes with Filament head
on**, and which currently pays for that competition out of its own pocket.

That is the real decision this branch has to make, and it is not "should we use
Filament." It is: **which half of SumiC survives contact with Filament, and which
half was never the differentiator.**

The answer this document argues for: SumiC keeps its frontend and its parity
guarantee, and hands its backend to `matc`.

---

## 1. Inventory — what actually exists today

Measured, not assumed.

### `eshi/` (C++, this repo)

| Piece | Size | State |
|---|---|---|
| `main.cpp` | 357 LOC | Shadertoy host: CPU/CUDA/GL/Metal dispatch, SDL2 window, FFmpeg encode |
| `renderer_cpu.h` | 52 | OpenMP `mainImage` loop. **This is the Ink tier.** |
| `renderer_gl.h` / `_metal.mm` / `_gpu.cu` | 325 / 274 / 95 | Per-backend fullscreen-quad shader hosts |
| `physics.h`, `input.h`, `game_interface.h` | 43 / 46 / 12 | "Hybrid Engine v1" — Pong-shaped, not general |
| `examples/*.cpp` | 20 shaders | The gallery. Every one is `mainImage(fragColor, fragCoord, iResolution, iTime)`. |
| `libsumi/` (C++) | 1,245 LOC header-only | vec/mat/quat/noise/SDF, CPU + CUDA + Metal-native types |
| `build.zig` | 221 | Builds `eshi` + every example; Metal on macOS; pkg-config for SDL2/FFmpeg |

Pong lives on `origin/feature/pong-engine` and is the thing to fix: the game is
written **inside `main.cpp`** as a `PongGame : IGame` class next to the event
loop, the state machine, and the renderer selection. `physics.h` exports
`ResolvePaddleBounce()` — a function whose name is its own indictment.

### `resources/gyosho/` (Rust, currently an untracked loose copy)

| Crate | Size | Role |
|---|---|---|
| `sumic` frontend — `lexer.rs`, `parser.rs`, `ast.rs`, `preprocessor.rs` | **632 LOC** | S2L → AST, with a working `#include` resolver |
| `sumic` backend — `codegen.rs` | **229 LOC** | Hand-written `MetalGenerator` + `WgslGenerator`; `MarkdownGenerator` is a stub |
| `libsumi` (Rust) | 338 LOC | glam-backed math, `no_std`-friendly, + `sdf.rs`, `noise.rs`, `camera.rs`, **`kantei.rs`** |
| `hanga` | 299 LOC | winit + wgpu live preview runtime |
| `examples/*.sumi` | 12 shaders | S2L corpus, incl. a large SDF primitive library |

Two observations that drive everything below.

**One.** The investment ratio is **632 frontend : 229 backend**. The language is
where the work is. The code generator is the thin, replaceable part — and
`Cargo.toml` already declares `naga = { features = ["glsl-in", "msl-out",
"wgsl-out"] }`, which is the author reaching for someone else's backend and then
hand-writing one anyway. That instinct was right. Filament is the better version
of that instinct.

**Two.** `libsumi/src/kantei.rs` is the most valuable file in the monorepo and the
proposal doesn't mention it.

---

## 2. Kantei is the spine — and it means Filament is *not* "the" renderer

`kantei.rs` (判定, *judgement/appraisal*) already defines a hardware capability
ladder, and it already names this engine:

| Grade | Target | Filament reaches it? |
|---|---|---|
| **1 · Ink** (墨) | Pure CPU / software raster. *"ESP32, Embedded, Playdate, Server (Eshi)"* | **No.** Filament's floor is GLES 2.0. |
| **2 · Paper** (紙) | GLES 2.0 / WebGL1 — Pi 4, old Android | Yes — FeatureLevel 0 |
| **3 · Brush** (筆) | Metal / Vulkan / DX12 | Yes — its home turf |
| **4 · Gold** (金) | RT cores, Neural Engine | Partly; NPU not at all |

The original Phase 1 says Filament is a **"drop-in replacement for the low-level
rendering backend."** Taken literally that deletes the Ink tier — and with it the
CPU/OpenMP renderer, the entire 20-shader gallery, the headless MP4 pipeline, and
every embedded target Kantei was written to describe. That is a large amount of
working software traded away for a dependency.

**Revised position:** Filament is the **Paper/Brush/Gold backend**. The existing
CPU renderer is the **Ink backend**. `Kantei::Grade` is the portable vocabulary
that selects between them, and it extends *below* Filament's floor — which is
precisely the thing Filament cannot buy you and the thing that makes Larimar
defensible against an engine that only ships when there's a GPU.

> Verify before committing to the mapping: Filament's `Engine::FeatureLevel`
> enum values and their exact GLES/Vulkan guarantees. The Kantei↔FeatureLevel
> correspondence above is a design intent, not a checked fact.

Practical consequence: `eshi_*` — the public C API — must never name Filament in
a signature. The backend is selected at init from a requested Grade, and every
renderer is reachable through the same entry points. That single rule is what
keeps the gallery alive through the transition.

---

## 3. The central reframe: SumiC becomes a frontend

### What SumiC does today

```
S2L ──▶ lexer ──▶ parser ──▶ AST ──▶ MetalGenerator ──▶ MSL
                                 └──▶ WgslGenerator  ──▶ WGSL ──▶ hanga (wgpu)
```

SumiC owns the whole pipeline. To be competitive that means eventually owning:
per-backend codegen for MSL/WGSL/SPIR-V/GLSL/ESSL, **shader variant
permutation** (lit/unlit × shadows × skinning × fog × MSAA × …), uniform and
resource binding layout across three binding models, a package container format,
and a runtime to host all of it. That is the commodity 80% of a shading toolchain
and it is maintained elsewhere by full teams.

### What SumiC does under Larimar

```
S2L ──▶ lexer ──▶ parser ──▶ AST ──▶ FilamentGenerator ──▶ .mat ──▶ matc ──▶ .filamat
                                 ├──▶ WgslGenerator ──▶ hanga        (authoring preview)
                                 └──▶ CppGenerator  ──▶ mainImage()  (Ink tier)
```

`matc` absorbs the backend fan-out, the variant permutation, the binding layout,
and the container format. SumiC keeps the three things that were *actually*
differentiated and that Filament has no opinion about:

1. **The S2L language itself** — pythonic ergonomics, `#include` composition, the
   SDF standard library. Filament materials are written in raw GLSL ES; nobody
   is competing with you on ergonomics there.
2. **CPU/GPU parity** — the `libsumi` promise that `dot()` is bit-identical on
   the processor and the shader core. Filament has no story for this at all, and
   it is the only reason the Ink tier can be trusted as a reference oracle.
3. **Kantei-aware compilation** — refusing at compile time to emit a shader that
   the declared target Grade cannot execute. `matc` will happily hand you a
   material your Pi 4 chokes on; S2L can reject it in CI.

The `CppGenerator` in that diagram is new and is the quiet win: S2L compiling
*down to the existing `mainImage` C++ signature* means one shader source feeds
both the Filament path and the OpenMP path, and the two can be pixel-diffed
against each other. That is a conformance oracle that most shading languages
would like to have and can't build.

### What this costs

`MetalGenerator` and `WgslGenerator` are **not** deleted. WGSL keeps `hanga`
alive as the fast authoring loop — wgpu is a far lighter dependency than Filament
for a live-coding feedback cycle, and you do not want a `matc` invocation between
a keystroke and a pixel. Metal-direct stays as the escape hatch for the Gold-tier
work in §7 where Filament's material model is the thing in the way.

### Constraint that must be designed around

`.mat` fragment blocks are **not free-form shaders**. Filament owns the vertex
stage, the varyings, the uniform block layout, and the lighting; a fragment block
fills in a `MaterialInputs` struct under a declared shading model. So:

- Gallery shaders (`mainImage`) map to **`shadingModel : unlit`** on a fullscreen
  quad, or to the **postprocess** material domain. This is a clean, direct fit.
- Surface materials get whatever subset of S2L survives that constraint.
- S2L features with no `.mat` expression must fail loudly at compile time with a
  Kantei-style diagnostic, not silently degrade.

> Verify: exact `.mat` grammar for `material {}` parameter declarations, the
> `postProcess` domain's inputs/outputs, and whether custom varyings are
> reachable from the unlit model. Design here is provisional until checked
> against a real `matc`.

---

## 4. Language boundaries — Rust is build-time only

Three languages is fine as long as exactly one of them is in the shipped binary's
call graph.

| Layer | Language | Ships in binary? |
|---|---|---|
| Toolchain — `sumic`, `hanga`, Rust `libsumi` | Rust | **No.** Dev machine + CI only. Emits `.filamat`, asset blobs, generated C++. |
| Core — ECS, physics, render bridge | C++ | Yes |
| Host — SDL3 app / Flutter embedder | C++ / Dart | Yes |
| Game — scene, components, systems | Dart | Yes |

This keeps the runtime FFI surface to **exactly one boundary** (C ABI between the
C++ core and Dart). No Rust FFI, no `cbindgen`, no second ABI to version. If
`hanga` were ever promoted to a shipping runtime that invariant breaks — so don't;
keep it a tool.

### Three math libraries is one too many

C++ `libsumi` (1,245 LOC), Rust `libsumi` (338 LOC), and `filament::math` all
exist. The Rust README says it is *"replacing legacy C++ `sumi.h`"* — that cannot
be true for a C++ runtime core. Proposed stance:

- **The contract is the S2L stdlib semantics.** Both libsumis are *implementations*
  of that contract and are conformance-tested against each other, driven by the
  `.sumi` corpus. Neither "replaces" the other; they serve different sides of the
  build.
- **`filament::math` stays inside the render bridge**, converted at the boundary.
  Both are column-major, so this is plausibly a `memcpy` — *verify, don't assume*,
  and put a static_assert on it.

---

## 5. Repository shape

```
eshi/                          # host apps, gallery, First Light games
  core/                        # → submodule: OS-oblivious C++ engine
    include/eshi/eshi.h        #   THE public surface. Pure C99. No Filament in any signature.
    src/ecs/                   #   SoA storage, entity ids, systems
    src/render/ink/            #   CPU/OpenMP backend (today's renderer_cpu.h)
    src/render/filament/       #   Paper/Brush/Gold backend
    src/scene/                 #   gltfio ingest, extras → colliders
  hosts/
    sdl/                       # standalone desktop host (SDL3)
    flutter/                   # embedder + external texture
  bindings/dart/               # dart:ffi package, ffigen'd from eshi.h
  third_party/filament/        # fetched prebuilt archives (scripts/vendor_filament.sh)
  examples/                    # the 20 shaders — must keep working throughout
gyosho/                        # → submodule: S2L, sumic, hanga, libsumi-rs
```

Note: `resources/gyosho` is currently an **untracked loose copy** with no `.git`
of its own. It should become a submodule or a `scripts/vendor.sh`-style sibling
checkout — the same pattern already used for `libsumi` — before any build step
depends on it. Its current location under `resources/` (which otherwise holds
`.gif` demo output) is also misleading.

### The boundary rule, concretely

- `core/` links Filament, libsumi, libc++. Nothing else.
- `core/` never calls SDL, never calls Flutter, never opens a window, has no `main()`.
- Hosts create the surface and own the event loop; core receives an opaque native
  handle it did not create.
- The public header is **C99**. This is not stylistic: one C header is consumed by
  `ffigen` for Dart *and* by Zig's `@cImport` with zero glue, which preserves the
  existing Zig build story for free. One header, three consumers.

---

## 6. Corrections to the original phases

Numbered so they can be argued with individually.

**6.1 — SumiC's premise, restated.** Covered in §3. The backend is the part to
give away. This is the whole point of the branch.

**6.2 — Filament is a tier, not a replacement.** Covered in §2. Phase 1's
"drop-in replacement" wording would cost the Ink tier, the gallery, and the
embedded story.

**6.3 — Phase 1 and Phase 3 describe two different hosts, not one pipeline.**
Phase 1 gives SDL3 the graphics context; Phase 3 has Filament rendering into a
Flutter-owned texture. Those are mutually exclusive at runtime — in the Flutter
path, Flutter owns the window and SDL is absent entirely. Model them explicitly
as **two hosts over one core**, which the boundary rule in §5 already permits.

**6.4 — Filament already has an ECS, and it is already SoA.** `utils::Entity`
plus `TransformManager` / `RenderableManager` are dense structure-of-arrays with
component indices. A second ECS that also stores Transform will double the memory
traffic that Phase 2's SoA argument exists to save, and will need a sync system
every frame. **Adopt `utils::Entity` as *the* entity ID**; let Eshi's ECS own only
gameplay components (Velocity, Collider, Script, Health) and write transforms
straight into `TransformManager`. The Ink backend implements the same interface
over its own arrays.

**6.5 — Filament already parses glTF, `extras` included.** `gltfio` is
spec-conformant and maintained; `FilamentAsset` exposes per-entity `extras` JSON.
Phase 4's hand-rolled parser duplicates it. **Keep the custom half** — reading
`trigger_door` out of `extras`, synthesising invisible colliders, routing events
back over FFI — and drop the parser.

**6.6 — Hot reload is the hard part and Phase 3 underestimates it.** Dart hot
reload re-executes `build()`; it does **not** unwind native state. Imperative
scene construction (`eshi_create_entity()` in `initState`) means a reload leaves
the old entities alive and creates a *second set*. Every reload doubles the
scene. The fix is to make the Dart layer **declarative and retained**: Dart
produces a scene *description*, a reconciler diffs it against the live ECS and
emits create/update/destroy ops keyed by stable IDs, gated on an epoch counter.
This is Flutter's own Widget→Element→RenderObject model applied across FFI — which
is a good sign it's the right shape, and a warning about how much machinery it is.
Budget it as a first-class subsystem, not a Phase 3 bullet.

**6.7 — Per-entity FFI calls per frame will eat the data-oriented win.** A
`dart:ffi` leaf call is cheap but not free, and thousands per frame plus the
attendant GC pressure is exactly the cache-and-overhead problem Phase 2 set out
to solve. **Use a shared command buffer**: allocate native memory once, expose it
to Dart as a `Float32List`/`Int32List` via `asTypedList`, let Dart write packed
commands into it, and cross the boundary **once** per frame with
`eshi_flush(count)`. Same trick in reverse for the event queue. Bulk in, bulk out.

**6.8 — Don't build Filament from source in-tree.** It is a large CMake project
with its own toolchain expectations and would dominate the build. Consume the
official per-platform prebuilt release archives via a fetch script following the
existing `scripts/vendor.sh` pattern. License is Apache-2.0, compatible with
eshi's MIT — but the NOTICE obligations are real and need a `third_party/`
attribution file.

**6.9 — Terminology.** `matc` is the *compiler*; the input is a `.mat` file, the
output is a `.filamat` package. "Output Filament's `matc` syntax" should read
"emit `.mat` and invoke `matc`."

**6.10 — SDL3 is nearly free here.** This machine's `sdl2` is **`sdl2-compat`
2.32.70** — an SDL3 shim — and `sdl3` 3.4.14 is already installed. Eshi is
already running on SDL3 in practice. The port is `SDL_INIT_*` flags, the
`SDL_CreateWindow` signature, and event-struct renames. Cheap; schedule it early
rather than treating it as a Phase 1 risk.

**6.11 — The Flutter texture path is per-platform, not one API.** External
textures exist on all targets but through different types — `FlutterTexture` /
IOSurface on macOS, `TextureRegistrar` with a D3D11 GPU-surface descriptor on
Windows, `FlTextureGL` on Linux, `SurfaceTexture` on Android. Phase 3's "pass the
texture ID" is four implementations. Plan for that; the desktop ones are the
under-documented ones.

**6.12 — The NPU claim needs to be dropped or rescoped.** Phase 4 argues the
SumiC pipeline gives an edge on ARM64 "where maximizing NPU and hardware
acceleration is critical." Filament does not target NPUs, and neither do MSL or
WGSL fragment shaders. NPU access is CoreML / NNAPI / vendor SDKs — a separate
compute path, not a shading-language backend. Either rescope this to *"efficient
GLES/Metal output for constrained ARM64 GPUs"* (true, defensible, and what
Kantei's Paper tier is for), or spin the NPU work out as an explicit Gold-tier
compute sidecar with its own design. As written it will not survive scrutiny.

---

## 7. Revised phase plan

The single biggest change: **First Light moves from last to first.** The proposal
places Pong at the end, after Filament, ECS, FFI, and the asset pipeline all
land. But Pong-on-an-abstracted-engine can be proven on the **Ink** backend with
no Filament at all — which validates the C API, the ECS layout, the command
buffer, and the host boundary while there is still only one moving part. Then the
render backend swaps underneath a proven interface.

Building the hardest dependency first and the acceptance test last is how you
discover in month six that the API shape was wrong.

### Phase 0 — Carve the boundary (no Filament)
- `core/` + `include/eshi/eshi.h` as pure C99.
- SoA ECS; entity IDs; component arrays; a system scheduler.
- Command ring buffer (§6.7) — exercised from C++ first, Dart later.
- `hosts/sdl` standalone host wrapping today's SDL2 path.
- **Ink** backend = today's `renderer_cpu.h` behind the new interface.
- Port Pong: game code becomes components + one system + a scene description.
- ✅ **First Light, Ink tier.**

### Phase 1 — Filament as a backend
- `scripts/vendor_filament.sh` fetching prebuilts (§6.8).
- `src/render/filament/` implementing the same interface as Ink.
- Kantei grade selection at init (§2).
- Gallery shaders as fullscreen unlit `.mat` — hand-written at this stage, to
  learn the constraints before automating them.
- ✅ Same Pong binary, `--grade brush`, unchanged game code.

### Phase 2 — SDL3 + host hardening (§6.10)

### Phase 3 — Dart
- `ffigen` bindings from `eshi.h`.
- Scene reconciler + epoch gating (§6.6) — the real work of this phase.
- Flutter embedder + external texture, macOS first (§6.11).
- ✅ Pong's paddle speed edited in Dart, hot-reloaded, no restart, no duplicate
  entities.

### Phase 4 — SumiC retargets to Filament
- `FilamentGenerator` in `sumic/src/codegen.rs`, emitting `.mat`.
- `CppGenerator` emitting the `mainImage` signature for Ink.
- **Acceptance corpus: the 20 existing `examples/*.cpp` shaders, ported to
  `.sumi`.** They already have known-good `.gif` output in `resources/`, which
  makes them a pixel-diff oracle for free. This is the cheapest real test suite
  available and it validates the parity claim in §3 directly.
- Kantei-aware compile-time rejection.

### Phase 5 — Assets
- `gltfio` + `extras` → colliders → FFI event bus (§6.5).

---

## 8. First Light — definition of done

Pong, with **zero engine code in the game**:

- [ ] `examples/pong/` contains a scene description, component data, and one
      system. No `main()`, no SDL call, no renderer reference, no `IGame`.
- [ ] Collision comes from a generic `Collider` component and a generic collision
      system. `ResolvePaddleBounce()` does not exist.
- [ ] The same game source runs under **both** hosts — `hosts/sdl` and
      `hosts/flutter` — unmodified.
- [ ] The same game source runs on **both** backends — Ink and Filament —
      selected at runtime by Kantei grade.
- [ ] Editing paddle speed in Dart and hot-reloading changes it live, and the
      scene does **not** duplicate (§6.6).
- [ ] All 20 gallery examples still render, on every phase, throughout.

That last checkbox is the regression gate for the whole project. If a phase
breaks the gallery, the phase is wrong.

---

## 9. Open questions

These need answers before Phase 1, and two of them are load-bearing:

1. **Does the Ink tier survive long-term, or is it a transitional oracle?** §2
   argues it is the defensible differentiator. If it is instead scaffolding to be
   dropped once Filament lands, §7's whole ordering changes and Kantei's Grade 1
   becomes vestigial. This is the biggest strategic question on the branch.
2. **Which `libsumi` is canonical**, and who owns the conformance suite that keeps
   them honest (§4)?
3. **Does `hanga` stay a preview tool, or become the Ink-tier runtime?** §4 assumes
   tool. Promoting it adds a second shipped ABI.
4. Is the Flutter host a *target* or *the* target? It determines whether the SDL
   host is a first-class product or a test harness.
5. Filament `FeatureLevel` ↔ Kantei `Grade` — verify the mapping in §2.
6. `.mat` expressive limits vs. the S2L corpus — verify before designing
   `FilamentGenerator` (§3).
