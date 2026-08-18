# Larimar Hero Asset

> Status: presentation prototype landed in
> `examples/larimar_flutter/lib/hero.dart`; the caustic surface material and
> general ECS `AngularVelocity` component remain Step 8 work. This is the detail
> record for that step; [`PLAN.md`](../../PLAN.md) holds the final gate and
> [`ARCHITECTURE.md`](ARCHITECTURE.md) holds the decisions it rests on.

## 1. Outcome

The hero asset is a spinning, refractive Larimar stone rendered inside an
`EshiView` with ordinary Flutter UI above it. It is the release's one image, and
it is chosen because it cannot be faked: producing it requires every layer of the
engine to work at the same time.

1. Faceted glTF geometry loaded natively through `gltfio`.
2. A generative caustic material authored once, compiled to `.filamat`.
3. Rotation integrated by the C++ ECS, with no per-frame FFI traffic.
4. Composition over Flutter UI through a transparent external texture.

It answers presentation quality directly, which is what a competing framework's
landing page asserts and a benchmark table cannot.

## 2. Evidence shaping the plan

- Fluorite leads with a spinning refractive asset because it demonstrates engine
  viability in one glance. The response has to be an equivalent artifact, not an
  argument.
- `gltfio` owns Filament's glTF parsing and instantiation. Bypassing it for one
  asset would fragment the resource pipeline the rest of RC0 depends on.
- Crossing the FFI boundary per frame to spin a transform would spend exactly the
  budget the shared command buffer exists to save. The intent belongs to Dart;
  the integration belongs to the core.
- The gallery's Voronoi program already models cellular caustics on the CPU and
  the GPU, within 1 LSB. That is the starting point for the stone's material, and
  it is already conformance-tested.

## 3. Scope

### Required

- `larimar_logo.glb`, a low-poly faceted gem — also Step 5's reference asset, so
  the release carries one asset rather than a test one and a pretty one.
- A caustic material mapping Voronoi noise to `baseColor`, `clearCoat`, and
  transmission.
- An `AngularVelocity` component and its system in the C++ core.
- Declarative Dart support for spawning the asset and declaring its spin.
- Transparent clear color through `EshiView` and the Filament view.

### Deferred

- Touch picking on the logo. Step 7 proves picking on a tagged trigger; the hero
  asset does not need to repeat it.
- Bloom, glare, and other post-processing beyond the material itself.
- Gold-tier or NPU work for this asset.

## 4. Ordered execution

Each stage ends in something checkable. Stage 1 lands with Step 5 rather than
here, because the reference asset and the hero asset are the same file.

### Stage 1 — Geometry (with Step 5)

- Model the faceted gem in Blender; export `larimar_logo.glb`.
- Load it through `gltfio` and expose instantiation by name over the command
  protocol.

**Check:** the greybox renders in `EshiView` when Dart asks for it — which is
Step 5's gate, met by the hero asset instead of a throwaway cube.

### Stage 2 — The caustic material

- Port the gallery's Voronoi logic into the material source, against the subset
  Step 6a specifies.
- Map the noise to `baseColor`, `clearCoat`, and transmission under a lit
  shading model.

**Check:** the static asset shows the cyan-and-white refractive material.

This is the first *surface* material in the project. Step 6a gates fullscreen
unlit materials, where a `mainImage` maps cleanly onto a device-domain quad; a
lit surface material fills in a different part of `MaterialInputs` and may need
subset rules the gallery never exercised. Anything the subset cannot express
must fail at compile time with a diagnostic, not degrade into a flat surface.

### Stage 3 — Rotation in the core

- Define an `AngularVelocity` POD component.
- Integrate it against the fixed timestep in a pure system, writing the rotation
  through the transform manager.
- Extend the command protocol with the component declaration.

**Check:** a C++ unit test advances an entity 60 ticks and asserts the resulting
transform, with no host involved.

### Stage 4 — Composition

- Declare the entity through the retained scene API with its mesh, material, and
  angular velocity.
- Configure a transparent clear color on the view and the Filament swapchain.
- Layer the `EshiView` beneath Flutter text and controls.

**Check:** the logo spins under the UI; one FFI call declares it and none occur
per frame while it turns.

## 5. Scorecard

| Area | Evidence | Current state |
|---|---|---|
| glTF pipeline | `resources/larimar-model.glb` loaded and instantiated by Dart | Prototype complete |
| Custom material | Caustic material compiled and bound | Unstarted |
| Native motion | One-time `spinY` declaration; general `AngularVelocity` ECS | Prototype complete; ECS remains |
| Composition | Transparent Filament `EshiView` under Flutter UI | Prototype complete |

The prototype also loops `resources/larimar-official.mp3` in the Flutter host.
Its 4.54-second spectral rise, shimmer, and decay keyframe the background color
story. This does not introduce an engine audio API; audio remains deferred RC
scope, as the architecture record requires.

## 6. Decision rules and risks

- **Simulation stays native.** A Flutter `AnimationController` spinning the logo
  would look identical and prove nothing. Dart declares that it spins at a rate;
  the core owns advancing the matrix. If that distinction is ever invisible in
  the profile, the demo has stopped demonstrating the claim.
- **Opaque fallback is acceptable.** If transparent external textures cost
  measurable composition time on macOS, match the view's clear color to the
  Flutter background instead and say so. A 60fps opaque hero beats a transparent
  one that stutters.
- **Parity is not optional.** The material must also emit its CPU entry point, so
  the logo remains renderable headless on Ink even when nobody looks at it that
  way. That path is what keeps the material inside the conformance oracle.
