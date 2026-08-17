# Eshi Engine Architecture Proposal

> The original vision document for **Larimar**, recorded verbatim. This is the
> north star. For the engineering plan — including where reality diverges from
> this document — see [ARCHITECTURE.md](ARCHITECTURE.md).

## the general direction of eshi moving forward to better envision S2L and the SumiC compiler thru Google Filament. "Larimar" a fluorite engine competitor.

## Executive Summary

This proposal outlines the strategic roadmap for transitioning **Eshi** into a
high-performance, cross-platform engine capable of rivaling modern embedded
graphics frameworks like Toyota's Fluorite. By adopting a "sandwich"
architecture—combining a Dart/Flutter top-level API with a data-oriented C++
Entity-Component-System (ECS) core—Eshi will offer seamless scene-based hot
reloading while maximizing cache coherency and raw rendering performance.

## Phase 1: Core Rendering & Submodule Isolation

* **Filament Integration:** Implement Google's Filament as a drop-in replacement
  for the low-level rendering backend. Filament will handle Vulkan/Metal/OpenGL
  abstraction, executing physically based rendering (PBR) and hardware
  interactions.
* **Submodule Boundary:** Isolate the C++ core into a dedicated submodule. The
  core must remain oblivious to the host operating system, exposing only a pure C
  API or controlled C++ headers for lifecycle management.
* **Windowing I/O:** Utilize SDL3 as the cross-platform I/O layer to initialize
  the graphics context and capture raw input events.

## Phase 2: Data-Oriented ECS Architecture

Transition from a traditional class-based engine to a data-oriented ECS to
eliminate cache misses and accelerate frame times.

* **Entities (IDs):** Represent game objects as simple 32-bit or 64-bit integer IDs.
* **Components (POD):** Store pure data (e.g., Transform, Mesh, Velocity) with
  zero attached logic.
* **Systems (Logic):** Implement pure functions that iterate over arrays of
  components.
* **Memory Layout (SoA):** Pack components into a Structure of Arrays (SoA)
  contiguous memory layout, ensuring maximum CPU L1 cache pre-fetching during
  rendering iterations.

## Phase 3: The Embedder and FFI Bridge

* **EshiView (Hardware Texture):** Have Filament render directly into a
  hardware-backed framebuffer. Pass the texture ID to Flutter to be wrapped in a
  Dart `Texture` widget, allowing 2D UI to overlay the 3D scene effortlessly.
* **FFI Communications:** Construct a `dart:ffi` bridge where Dart acts as the
  scene graph orchestrator. Dart will call functions like `eshi_create_entity()`
  and update transforms.
* **Scene Hot Reloading:** Maintain all game logic, behavior scripts, and scene
  instantiation in Dart. Because C++ solely acts as a "dumb" renderer and physics
  calculator, Dart's native hot reload will instantly update the running 3D scene
  without native recompilation.

## Phase 4: Asset Pipeline and Toolchain Advantage

* **Interactive glTF Parsing:** Build a glTF parser in the C++ core capable of
  reading custom node metadata (e.g., `trigger_door`). Automatically generate
  invisible raycast colliders for these nodes that send events back across the
  FFI bridge to Dart listeners.
* **SumiC Shader Transpilation:** Leverage the existing custom compiler, SumiC, to
  ingest high-level or generative material definitions and output Filament's
  `matc` syntax. This bespoke pipeline will allow for highly optimized graphics
  targets, giving an edge on constrained ARM64 architectures where maximizing NPU
  and hardware acceleration is critical.

## First Light Goal

`feature/pong-engine` should no longer be such a hacked together example for
writing games in eshi, we should be able to produce at least this behavior, but
with abstracted game engine logic.

## Conclusion

By combining the developer velocity of Flutter and Dart with a strict,
cache-coherent C++ ECS and Filament's rendering power, Eshi will become a premier
rendering environment capable of executing highly optimized, interactive 3D
experiences.
