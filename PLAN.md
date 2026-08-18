# Larimar Release Candidate Plan

> Status: active on `feat/larimar` at baseline `8f915e6` (2026-08-17).
> This is the delivery plan. [`docs/larimar/ARCHITECTURE.md`](docs/larimar/ARCHITECTURE.md)
> remains the design record and explains the decisions behind it.

## 1. Release outcome

Larimar RC0 is an installable Flutter package and native engine that proves one
complete product slice:

1. A game describes a retained scene in Dart.
2. A shared command buffer crosses FFI once per frame into Larimar's C++ ECS.
3. Filament renders a PBR glTF scene into an `EshiView` Flutter widget.
4. Editing the Dart scene hot-reloads without duplicating entities or resetting
   simulation-owned state.
5. Two `EshiView` widgets can show the same world through independent cameras
   while sharing entity state with ordinary Flutter UI.
6. A Blender-authored tag in glTF `extras` produces a hit event that Dart can
   handle.
7. The same core still runs Pong and the shader gallery through the Ink, Paper,
   and Brush grades, including deterministic headless rendering and video
   export.
8. One authored material source produces both the Filament material and the CPU
   entry point, with no shader maintained twice and the tiers agreeing to 1 LSB.
9. A hero asset — a spinning, refractive logo — renders that whole stack as one
   image: glTF geometry, a compiled material, native rotation, Flutter UI above.

That is the smallest honest release candidate comparable in nature to
[Fluorite's public description](https://fluorite.game/): Dart authoring, a
data-oriented native ECS, Filament rendering, Flutter composition, hot reload,
multi-view state sharing, and artist-authored touch regions. Larimar's distinct
claim is its capability ladder below Filament's GPU floor plus deterministic
headless/offline rendering. RC0 must demonstrate that difference; it must not
claim shipping console support or superiority that has not been measured.

## 2. Evidence shaping the plan

- Fluorite publicly emphasizes a C++ ECS, Dart game code, a `FluoriteView`,
  Filament PBR, multiple simultaneous views, hot reload, and model-authored
  touch zones. These define the competitive acceptance slice, not an obligation
  to copy its implementation.
- Toyota Connected's public
  [`ivi-homescreen`](https://github.com/toyota-connected/ivi-homescreen) work
  now exposes Flutter compositor surfaces, EGL/Vulkan backends, and optional
  DMA-BUF export suitable for zero-copy consumers such as Filament. This makes
  the host/render-surface boundary and explicit texture ownership strategically
  important for Larimar; it does not justify coupling the core to that embedder.
- Flutter texture IDs are scoped to a Flutter view and come from a
  platform-specific registry. Multi-view is therefore a host resource-lifetime
  problem, not merely a second Dart widget.
- Filament's `gltfio` already owns glTF/GLB parsing, resources, instances, and
  renderable entities. Larimar should add gameplay meaning to `extras`, not
  build another parser.

## 3. RC0 scope

### Required

- macOS Apple Silicon as the reference Flutter/Metal platform.
- Linux x86_64 for the OS-oblivious core and OpenGL/Ink gates.
- A versioned C ABI, generated Dart bindings, command/event codecs, and clear
  ownership rules for every native allocation.
- A macOS external-texture implementation with resize, disposal, device-loss,
  and application lifecycle handling.
- Filament mesh/renderable, transform, camera, light, material, and glTF asset
  support sufficient for one small PBR scene.
- A dual-target material compiler: one authored source emits both a `.mat`
  definition that `matc` compiles for Filament and the `mainImage` entry point
  the Ink tier executes, with the shader subset written down as a specification.
- Retained Dart scene declarations, live hot reload, shared state, two views,
  picking, and one glTF `extras` trigger event.
- Automated native, Dart, widget, integration, conformance, and smoke tests.
- Versioned artifacts, licenses/NOTICE, an example app, quick-start docs,
  changelog, known limitations, and rollback instructions.

### Deferred beyond RC0

- Console SDK integration, certification, or a public "runs on consoles" claim.
- Windows, iOS, Android, Web, and production Embedded Linux hosts.
- SDL3 migration unless it blocks an RC gate; SDL2 remains a supported harness.
- Jolt/3D physics, audio redesign, Gold/NPU work, an editor, and a broadphase
  rewrite.
- General-purpose 3D coverage beyond the reference glTF vertical slice.
- S2L as an authoring language for anything outside the gallery corpus, and the
  WGSL/`hanga` live-coding loop. Step 6b brings the language in as a source
  generator; making it the only way to author a Larimar material does not
  belong in RC0.

## 4. Ordered execution plan

Each step ends in an independently checkable gate. A later step may start only
when its dependency and gate are green.

### Step 1 — Restore and freeze the release gate — **complete**

The first run for `8f915e6` failed before repository code executed. All three
jobs used `mlugg/setup-zig@v1`, which requested stable Zig 0.16.0 from the
nightly `/builds` location and received HTTP 404. Upgrade all jobs to the
maintained `@v2` action while retaining the project's tested Zig 0.16.0 pin.

- [x] Diagnose [run 32055999900](https://github.com/crux161/eshi/actions/runs/32055999900).
- [x] Update all three setup steps from `mlugg/setup-zig@v1` to `@v2`.
- [x] Run `zig build test` locally with Zig 0.16.0 (132 checks, 0 failures).
- [x] Push the fix and require all CI jobs to execute project code and pass —
      [run 32058143629](https://github.com/crux161/eshi/actions/runs/32058143629)
      is green on commit `2c2c8288`.
- [x] Protect `feat/larimar` with strict GitHub Actions checks, enforced for
      admins; disable force-pushes and branch deletion.

Gate met: core, OpenGL, and Metal jobs all execute project code and pass, and
GitHub will not update the RC integration branch unless those checks are green.

### Step 2 — Freeze the native contract — **complete**

Dependency: Step 1.

- [x] Add an ABI/version query and reject incompatible Dart/native pairs early.
- [x] Audit `eshi.h` for C99 portability, struct sizes/alignment, error
      propagation, thread affinity, and create/destroy symmetry.
- [x] Turn the packed command and event formats into a versioned specification
      with golden byte fixtures shared by C++ and Dart.
- [x] Add malformed-buffer, stale-epoch, capacity, and lifecycle tests plus
      ASan/UBSan coverage on Linux.

Gate met: `zig build abi-test` dynamically loads the release-mode library and
validates every public layout (187 checks); the core round-trips both golden
streams and hostile inputs (238 checks), and `zig build test-sanitize` reports
no ASan/UBSan findings.

### Step 3 — Create the Dart/Flutter package — **complete**

Dependency: Step 2.

- [x] Scaffold `bindings/dart/larimar` as a Flutter FFI plugin and
  `examples/larimar_flutter` as the reference application.
- [x] Generate bindings from `core/include/eshi/eshi.h` with `ffigen`; check in the
  generated file and add a CI drift check.
- [x] Implement safe Dart ownership wrappers, typed command/event views, epochs,
  error mapping, and deterministic disposal.
- [x] Mirror C++ golden fixtures in Dart tests before building widgets.

Gate met: [run 32064758876](https://github.com/crux161/eshi/actions/runs/32064758876)
passes the binding-drift check, `dart analyze`, nine Dart tests including a real
native code-asset load/flush/tick/drain smoke, and the Flutter reference app's
analysis/widget test on Linux x86_64 and macOS Apple Silicon. Both Dart FFI jobs
are strict required checks on `feat/larimar`; the scaffolded macOS application
also builds locally with the native library embedded.

### Step 4 — Land `EshiView` on macOS — **complete**

Dependency: Step 3.

- [x] Implement a macOS Flutter texture registrar adapter backed by a Metal texture
  that Larimar/Filament can render into without CPU framebuffer readback.
- [x] Keep the texture target per view and the world shareable. Camera and
  Filament swapchain ownership lands with Step 5's scene backend; the current
  fullscreen Brush backend owns neither resource. Never expose platform or
  Filament types through `eshi.h`.
- [x] Specify render-thread ownership and marshal resize, frame-available, app
  suspend/resume, and disposal operations to it.
- [x] First render existing Pong through the widget; this isolates host/texture
  correctness from unfinished 3D scene work.
- [x] Exercise five real create/resize/pause/resume/destroy cycles in the macOS
  integration runner and cover delayed creation/disposal with widget tests.
- [x] Run the real lifecycle loop under leak/race diagnostics and retain a
  captured visual artifact.

Gate met, in two halves, because one binary cannot answer both questions.

`scripts/check_eshiview_host.sh` builds the adapter twice from its real method
channel entry points and drives it from a platform thread against a stand-in
raster queue with no engine present: 24 create/resize/suspend/destroy cycles and
408 raster borrows report `0 leaks for 0 total leaked bytes` under
`leaks --atExit`, and no findings under ThreadSanitizer. Both passes fail when
they should — deleting `LarimarTexture`'s surface lock produces a race report at
the resize that swaps the buffer out from under an in-flight copy, and dropping
the `CVPixelBufferRelease` produces one rooted `CVPixelBuffer` per cycle.

`scripts/check_eshiview_lifecycle.sh` runs the same loop inside a real Flutter
engine. Twelve cycles of create, resize, background, foreground, and destroy end
with `created: 12, disposed: 12, registered: 0, live: 0, liveSurfaceBytes: 0`,
and `leaks`, snapshotted from outside the App Sandbox after the first cycle and
after the last, reports the same 14320 bytes both times — no growth. Removing
the widget's disposal call fails it at cycle 0.

The visual half took two attempts, and the first one was wrong. It captured the
app with `RepaintBoundary.toImage` and asserted Pong appeared inside the view
rect, which passed — until it didn't: on a later run the same assertion failed
with the surface holding a complete frame and the engine borrowing it sixty
times a second. `toImage` rasterizes the layer tree offscreen and does not
reliably include an external texture layer. A capture that can come back blank
while everything works proves nothing when it is bright, either, so both of
those earlier passes have to be read as luck.

What replaced it splits the claim in two, because it is two claims:

- *Larimar drew the frame.* The host reads its own surface back to a PNG —
  [`eshiview-pong-01.png`](docs/larimar/media/eshiview-pong-01.png) and
  [`-02`](docs/larimar/media/eshiview-pong-02.png), twenty frames apart, with
  the ball and paddles in different places. Peak luminance rules out a cleared
  surface; the difference between them rules out a frozen one.
- *Flutter composited it.* The engine borrows the surface exactly when it draws
  the texture layer, so the host's borrow count is the only instrument that can
  answer this, and the test now waits on it rather than on a fixed delay.

Running the application then found what neither half covered: the paddles were
outside the window. The material works in `uv = (fragCoord * 2 - iResolution) /
iResolution.y`, so the *horizontal* extent of the scene is the view's aspect
ratio and only the vertical one is fixed; the game placed its paddles at the
16:9 arena's edges, which is off screen in the 4:3 window Flutter opens by
default. The demo videos are 16:9 and showed nothing wrong. The reference
application now takes its arena width from the view, and the visual gate runs at
4:3 and asserts something bright at each edge — a test that would have failed
before the fix and does, when the constant is put back.

Playing it found the rest, none of which any pixel or lifecycle gate covers:
the Flutter application had no input at all, its keys reached AppKit unhandled
and beeped, and a match never ended — the score ran past the nine dots the
material can draw and kept counting. Both paddles now take the keyboard (W/S and
the arrows, matching the SDL host), the key handler claims what it uses, and a
match plays to nine and announces its winner. That last one has a gate: an
integration test plays a one-point match and requires the announcement to appear
and then clear. The C++ Pong has the same missing win condition and keeps it for
now; its deterministic digests are what several other gates compare.

Not gated, and recorded as such: `footprint` reports the process's IOSurface
count rising by two across the run. That number also covers the engine's own
swapchain surfaces, so it cannot isolate the view's, and the in-process
accounting above answers the question it was meant to answer.

### Step 5 — Promote Filament First Light into a 3D scene backend — **in progress**

Dependency: Step 4.

- [x] Vendor a pinned official Filament distribution with checksums and
  Apache-2.0 NOTICE compliance; do not build Filament from source in ordinary
  consumer builds. `scripts/vendor_filament.sh` fetches v1.75.0, verifies its
  SHA-256, and extracts it to `third_party/filament/<version>`, which is now
  `-Dfilament-path`'s default — a build cannot silently link a different
  Filament than the one its materials were compiled by. Attribution lives in
  [`third_party/NOTICE.md`](third_party/NOTICE.md).
- [x] Add asset handles to the internal backend interface and the public ABI
  without leaking Filament into it. `EshiAsset` is an opaque `uint32_t`;
  `eshi_asset_load`, `_instance`, `_release` and `_count` route to four optional
  vtable entries that a backend without a scene simply leaves null. The ABI
  minor moved to 1.1.0 — additive, so a package built against 1.0 still loads.
- [x] Use `gltfio` for glTF/GLB parsing and resource loading; use instancing
  rather than duplicate asset uploads. Assets are created instanced from the
  start, so three copies of the logo report `instances=3 assets=1`.
- [x] Establish PBR defaults, units, camera convention, and deterministic asset
  failure behaviour: glTF's own right-handed Y-up metres, a 45° vertical field
  of view, photographic exposure, one daylight directional light, and an
  explicit opaque clear.
- [x] Use the hero asset's greybox `.glb` (Step 8) as the reference asset rather
  than authoring a throwaway one — `eshi-gemgen` writes it, for the same reason
  the materials are generated rather than checked in.
- [ ] Image-based lighting, which is why a fully metallic material still renders
  dark; the reference material is a dielectric so the gap is visible rather
  than disguised.
- [ ] Renderable/mesh, camera and light *commands*, so a Dart scene can place
  geometry rather than a host calling the C API directly.
- [ ] The `EshiView` half of the gate: one upload shared across two views.

Gate: the reference GLB renders lit PBR geometry through `EshiView`, assets are
uploaded once across two views, and missing/corrupt assets return actionable
errors rather than blank output or crashes.

### Step 6 — One material source, every tier

Dependency for 6a: Filament First Light, which landed — `matc` runs from
`build.zig` today and the Brush tier renders a `.filamat`. This step does *not*
wait on Step 5; the fullscreen material path it gates already exists, and
leaving it until after Step 5 would mean carrying a hand-maintained duplicate
through the 3D work.

The duplicate is the reason this step exists. `examples/pong/pong.mat` is a
hand-written transliteration of `examples/pong/pong.gpu.cpp`: same `sdBox`, same
`glow`, same constants, same score loop, two files to keep in step. Every
material added under Filament multiplies that. `core/src/render/transpile.h`
already calls itself "the de-facto S2L" and already treats the 20 gallery
programs as its conformance corpus; the missing emitter is `.mat`.

#### 6a — Emit `.mat` from the shader subset — **complete**

- [x] Write the subset down as a specification, closing
  [`ARCHITECTURE.md`](docs/larimar/ARCHITECTURE.md) §9 open question 5 —
  [`SHADER_SUBSET.md`](docs/larimar/SHADER_SUBSET.md). It is what 20 programs
  already conform to; the document is a description, not a redesign.
- [x] Add a `.mat` target to `transpile.cpp` beside GLSL and MSL, and a
  build-time generator (`eshi-matgen`) that writes the material for `matc`.
- [x] Generate `pong.filamat` from `pong.gpu.cpp` and delete the hand-written
  `pong.mat`. Refuse to emit, with a diagnostic naming the cause, for any
  construct the `.mat` fragment domain cannot express — never degrade silently.
- [x] Build the cross-tier comparison the plan had so far measured by hand:
  dump raw frames per tier and compare them, so "within 1 LSB" is a command
  rather than a table in a document.

Gate met. `examples/pong/pong.mat` is gone: the build runs `eshi-matgen` over
`pong.gpu.cpp` and pipes the result to `matc`, so Pong's material has one
definition again. The same pipeline gives `examples/ripple.cpp` a package, which
is what lets a gallery shader reach Filament at all — it previously failed with
"material has no package_path".

`scripts/check_tier_conformance.sh` renders both scenes on every tier the host
offers and compares raw frames: Pong and ripple, on Paper (OpenGL) and Brush
(Filament), against Ink — four comparisons, all `mean=0.0000/255 max=1/255` at
320x180 over 60 frames. It refuses to pass a tier that quietly fell back to Ink,
because comparing Ink to Ink would be green and meaningless.

`scripts/check_materials.sh` runs the emitter over the whole corpus: 18 of the
20 programs compile through `matc`. The other two refuse, and finding out *why*
was the useful part of this step:

- `warp.cpp` samples `iChannel0`, and the material declares no sampler.
- `rainforest.cpp` opens its march loop inside `#ifdef LOWQUALITY` and again
  inside the `#else`. Every compiler in the chain handles that; `matc` splits a
  `.mat` into blocks by counting braces *before* preprocessing, so the fragment
  block ran past its own closing brace and matc reported an unexpected character
  on an innocent line. The emitter now says that instead.

A third finding was fixable rather than a limit: Filament's prelude defines `PI`
and `HALF_PI`, which `lunar.cpp` and `seascape.cpp` also use, so the material
target renames them the way the GLSL target already renames the reserved
`noise1`–`noise4` builtins.

Fully gated in CI as of Step 5's vendoring bullet. `matc` is a build-time tool
that needs no GPU, so *both* the Linux and macOS jobs now compile every emitted
material, and the macOS job additionally renders Pong and ripple through
Filament and compares them against Ink. The macOS runner may have no Metal
device, so that last comparison is gated on the same probe the shader
validation uses.

Building something to *look* at found two defects none of the gates could. The
first was in the demo itself: `scripts/build_demo.sh` shipped whatever bundle
sat in the Flutter build directory, and `flutter test -d macos` writes its own
bundle there — with the integration test as the application's entry point. The
demo therefore shipped an app that ran the test suite, tore its widget tree
down, and left a blank window. It now builds the application explicitly.

The second was older:
`zig-out/bin/pong --grade paper` rendered pure black from any directory but the
repository root, printed one line about a missing shader source, and exited
zero, while the banner still announced `backend=gl`. The shader sources now
install to `zig-out/share/eshi/shaders` and resolve from there (or from
`ESHI_SHADER_DIR`), a tier that cannot bind its material refuses to run instead
of presenting black frames, and `zig build demo` renders both scenes on every
available tier into `build/demo` — with the Flutter application beside them —
so the question "does it still look right" has an answer that is not a digest.

#### 6b — S2L as an additive frontend

Dependencies: 6a, and `resources/gyosho` remaining a tool rather than a
dependency (§4 of the architecture record: Rust is build-time only).

- Give `sumic` a `CppGenerator` that emits the subset 6a specified, so an S2L
  material reaches Filament, OpenGL, Metal, and Ink through the path 6a already
  gates. Nothing in the runtime links Rust; CI checks the generated C++ in.
- Port a representative slice of the gallery — not all 20 — to `.sumi`, chosen
  to exercise the constructs the subset spec names.
- Reject at compile time what the declared Kantei grade cannot execute.

Gate: a `.sumi` source renders on Brush and Ink within 1 LSB through generated
C++, and a drift check fails if the checked-in generated source no longer
matches what `sumic` produces — the same gate shape as the `ffigen` bindings.

### Step 7 — Prove Dart hot reload, shared state, multi-view, and touch

Dependencies: Steps 3 and 5.

- Expose a declarative Dart scene API that emits the existing keyed scene
  commands; document the rule: describe what a scene is, let systems own what it
  is doing.
- Build the release demo with two cameras and ordinary Flutter controls bound to
  shared native entity state.
- On hot reload, advance the epoch, reconcile topology, preserve unchanged
  simulation state, and reject callbacks carrying stale epochs.
- Read a namespaced Larimar trigger object from glTF `extras`, build its picking
  proxy, and deliver a tagged event through the bulk event buffer to Dart.

Gate: an automated integration scenario hot-reloads the scene 100 times during
simulation, retains the expected entity/asset counts and state digest, shows
both views updating, and receives the expected tagged hit event.

### Step 8 — Cut the hero asset

Dependencies: Steps 5, 6, and 7. Detailed in
[`docs/larimar/HERO_ASSET.md`](docs/larimar/HERO_ASSET.md).

The spinning refractive logo is the release's one image, and it is also the
first thing in the plan that is *not* a fullscreen material: a caustic surface
shader on real geometry, driven by a native component, composited under Flutter
UI. Each of those is a capability Steps 5–7 build; this step is where they have
to hold together at once.

- Author `larimar_logo.glb` and use it as Step 5's reference asset from that
  step onward, so the release has one asset rather than a test one and a pretty
  one.
- Write the caustic material against the subset from Step 6, mapping Voronoi
  noise to `baseColor`, `clearCoat`, and transmission. This is the first surface
  material: the fullscreen unlit mapping Step 6 gates does not cover it, and
  what the subset cannot express must fail loudly.
- Add an `AngularVelocity` component and the system that integrates it, so
  rotation is simulation the core owns rather than an animation Dart drives.
- Give `EshiView` and the Filament view a transparent clear color, and layer
  the widget beneath ordinary Flutter controls.

Gate: the logo spins under Flutter UI with one FFI call to declare it and none
per frame; a C++ unit test shows `AngularVelocity` advancing a transform over 60
ticks with no host involvement; and a captured frame is retained the way Step 4's
composition evidence was.

### Step 9 — Harden the release candidate

Dependencies: Steps 1–8.

- Add a CI matrix for native core, Flutter package, example app, sanitizer,
  release-mode smoke, cross-tier pixel conformance, and package assembly.
- Record reproducible performance baselines: startup time, package size,
  resident memory, FFI flush time, asset-load time, median/P95/P99 frame time,
  and dropped frames on named reference hardware.
- Set regression budgets from that baseline and fail CI on statistically
  meaningful regressions; do not use "console-grade" as an unmeasured synonym
  for attractive output.
- Run 30-minute lifecycle/resize/hot-reload stress, malformed asset/command
  tests, leak checks, and clean-machine installation tests.
- Write a threat model for untrusted glTF/GLB and command buffers and document
  the trusted-content boundary for RC0.

Gate: every measurement is published as a build artifact, no critical defect is
open, and every required test is green from a clean checkout.

### Step 10 — Cut RC0

Dependency: Step 9.

- Select a SemVer prerelease (`0.1.0-rc.1`), freeze the ABI and dependencies,
  and create a release branch from a green commit.
- Publish macOS arm64 native artifacts, the Flutter package, symbols, checksums,
  SBOM, licenses/NOTICE, example source, changelog, and known limitations.
- Restrict release-tag creation to the gated release workflow so a red commit
  cannot be tagged directly.
- Verify the documented quick start in a clean environment, then tag the exact
  verified commit. Preserve a rollback path to the last green artifact set.

Gate: a new user can install the published artifacts and run the two-view PBR
hot-reload demo without a repository checkout or an undocumented dependency.

## 5. Release scorecard

| Area | RC0 evidence | Current state |
|---|---|---|
| Native ECS and deterministic simulation | 132 core checks; stable Pong digest | Implemented |
| Retained scene + bulk FFI transport | Reload invariant and epoch tests | Dart/native transport implemented; Flutter reload proof remains |
| Render capability ladder | Ink/Paper/Brush; 1-LSB conformance target | Implemented for fullscreen materials, and now checked by a command |
| Filament | Pong First Light through `.filamat` | Distribution pinned; glTF loads, instances and draws; IBL and scene commands remain |
| Dart API | Generated, version-checked package | Implemented and drift-gated |
| Flutter composition | macOS `EshiView` external texture | Implemented and gated on leak/race diagnostics |
| Custom materials | One source emits `.mat` and the Ink entry point | Fullscreen materials generated and gated; S2L frontend (6b) remains |
| Hot reload from Dart | 100-reload integration scenario | Missing |
| Multi-view/shared state | Two cameras, one world/assets | Missing |
| glTF PBR + touch tag | One reference GLB and event | Reference GLB renders lit PBR; `extras` event missing |
| Hero asset | Spinning refractive logo under Flutter UI | Missing |
| Release engineering | Green required CI and installable artifacts | Steps 1 and 3 gated; packaging remains |

## 6. Decision rules and risks

- **Protect the boundary.** SDL, Flutter, Metal, and Filament stay in adapters;
  the C++ core remains testable without OS, windowing, or media dependencies.
- **Prove one platform deeply.** macOS/Metal is the RC reference. New hosts wait
  until texture lifecycle and ABI behavior are stable.
- **Preserve the oracle.** Any 3D/Flutter work that breaks Ink, the gallery,
  deterministic Pong, or cross-tier conformance is not release progress.
- **No silent compatibility.** Unknown command opcodes, ABI mismatches, corrupt
  assets, and unavailable backends fail visibly with actionable diagnostics.
- **Avoid dependency drift.** Pin Zig, Flutter, Filament, and generator versions;
  record checksums and upgrade them in isolated changes with full gate runs.
- **Control RC scope.** Physics, audio, and additional platforms are important
  but cannot enter RC0 unless they close a required gate above.
- **One source per material, always.** A shader that exists twice is a bug with a
  delay fuse: the copies agree until someone edits one. Step 6 exists to remove
  the only such pair in the repository and to make adding another impossible.
- **The language is not the deliverable.** Step 6's outcome is one source
  reaching every tier. S2L makes that source nicer to write, which is why 6b
  follows 6a rather than gating it: the conformance corpus must not depend on a
  parser that is still being built.

The immediate next task is the rest of Step 5: image-based lighting, the scene
commands that let Dart place geometry rather than a host calling the C API, and
the two-view upload-sharing half of the gate. Loading, instancing and drawing a
glTF now work end to end, with `scripts/check_asset_render.sh` proving a drawn
frame differs from an empty one.
