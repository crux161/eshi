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
- Retained Dart scene declarations, live hot reload, shared state, two views,
  picking, and one glTF `extras` trigger event.
- Automated native, Dart, widget, integration, conformance, and smoke tests.
- Versioned artifacts, licenses/NOTICE, an example app, quick-start docs,
  changelog, known limitations, and rollback instructions.

### Deferred beyond RC0

- Console SDK integration, certification, or a public "runs on consoles" claim.
- Windows, iOS, Android, Web, and production Embedded Linux hosts.
- SDL3 migration unless it blocks an RC gate; SDL2 remains a supported harness.
- Jolt/3D physics, audio redesign, Gold/NPU work, SumiC retargeting, an editor,
  and a broadphase rewrite.
- General-purpose 3D coverage beyond the reference glTF vertical slice.

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

### Step 4 — Land `EshiView` on macOS

Dependency: Step 3.

- Implement a macOS Flutter texture registrar adapter backed by a Metal texture
  that Larimar/Filament can render into without CPU framebuffer readback.
- Keep the texture, camera, and swapchain per view; keep the world and assets
  shareable. Never expose platform or Filament types through `eshi.h`.
- Specify render-thread ownership and marshal resize, frame-available, app
  suspend/resume, and disposal operations to it.
- First render existing Pong through the widget; this isolates host/texture
  correctness from unfinished 3D scene work.

Gate: a widget integration test repeatedly creates, resizes, backgrounds,
foregrounds, and destroys `EshiView` under leak/race diagnostics, while a visual
smoke test shows animated Pong with Flutter UI layered above it.

### Step 5 — Promote Filament First Light into a 3D scene backend

Dependency: Step 4.

- Add renderable/mesh, camera, light, and asset handles to the internal backend
  interface and command protocol without leaking Filament into the public ABI.
- Use `gltfio` for glTF/GLB parsing and resource loading; use instancing rather
  than duplicate asset uploads.
- Establish PBR defaults, image-based lighting, color space, units, camera
  convention, and deterministic asset failure behavior.
- Vendor a pinned official Filament distribution with checksums and Apache-2.0
  NOTICE compliance; do not build Filament from source in ordinary consumer
  builds.

Gate: the reference GLB renders lit PBR geometry through `EshiView`, assets are
uploaded once across two views, and missing/corrupt assets return actionable
errors rather than blank output or crashes.

### Step 6 — Prove Dart hot reload, shared state, multi-view, and touch

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

### Step 7 — Harden the release candidate

Dependencies: Steps 1–6.

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

### Step 8 — Cut RC0

Dependency: Step 7.

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
| Render capability ladder | Ink/Paper/Brush; 1-LSB conformance target | Implemented for fullscreen materials |
| Filament | Pong First Light through `.filamat` | 3D scene work missing |
| Dart API | Generated, version-checked package | Implemented and drift-gated |
| Flutter composition | macOS `EshiView` external texture | Missing |
| Hot reload from Dart | 100-reload integration scenario | Missing |
| Multi-view/shared state | Two cameras, one world/assets | Missing |
| glTF PBR + touch tag | One reference GLB and event | Missing |
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
- **Control RC scope.** Physics, audio, SumiC, and additional platforms are
  important but cannot enter RC0 unless they close a required gate above.

The immediate next task after Step 1 turns green is Step 2: freeze the ABI and
wire-format fixtures before Dart code depends on them.
