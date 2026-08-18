# Larimar native contract v1

This document freezes the host-facing contract for RC0. The normative public
surface remains [`eshi.h`](../../core/include/eshi/eshi.h); this records the
rules a generated Dart binding cannot infer from declarations alone.

## Startup compatibility check

A host must perform these calls before creating a world or mapping shared
storage:

1. Call `eshi_abi_check(ESHI_ABI_VERSION,
   ESHI_COMMAND_PROTOCOL_VERSION, ESHI_EVENT_PROTOCOL_VERSION)` using the
   versions compiled into the host package.
2. Stop loading and report both package and native versions on
   `ESHI_ERR_VERSION`.
3. Call `eshi_abi_query()` with the host's `sizeof(EshiAbiInfo)`.
4. Validate the reported sizes and offsets before constructing FFI structs.

ABI compatibility requires the same major and a native minor greater than or
equal to the host's requested minor. Patch releases do not change layouts or
semantics. Command and event versions match exactly because silently skipping a
new opcode can produce a plausible but incorrect scene.

`EshiAbiInfo` is append-only within an ABI major. A short query copies only the
caller's capacity, writes the native size into its first four bytes, and returns
`ESHI_ERR_LIMIT`; it never writes beyond `out_size`.

## C99 and layout requirements

`eshi.h` is valid C99 and includes only `<stddef.h>` and `<stdint.h>`. The ABI
probe compiles it as C99 with pedantic diagnostics, dynamically links the
release-mode C++ library, and compares the C view with the native descriptor.
The release contract requires:

- 8-bit bytes and IEEE-754 binary32 `float`;
- 32-bit `EshiEntity`, public enums, and `EshiResult`;
- the pointer and `size_t` widths reported by `EshiAbiInfo`;
- the reported size, alignment, and every field offset for `EshiConfig`,
  `EshiTransformView`, `EshiVelocityView`, `EshiCollisionEvent`, and
  `EshiMaterial`.

Do not hard-code LP64 offsets in a binding generator. Generate declarations
from the header, then use the descriptor as a load-time assertion. A toolchain
using short enums or a different aggregate ABI fails the release probe.

## Ownership and lifetime

| Value | Owner | Lifetime / invalidation |
|---|---|---|
| `EshiWorld*` | Caller after successful `eshi_world_create()` | Exactly one `eshi_world_destroy()`; destroying `NULL` is safe. |
| Command words | World | Borrowed after `eshi_commands_data()`; invalidated by a later command reserve or world destruction. |
| Event words | World | Borrowed after `eshi_events_data()`; invalidated by a later event reserve or world destruction; contents are replaced by `eshi_events_pack()`. |
| Component views | World | Borrowed parallel arrays; invalidated when a component of that type is added or removed, or the world is destroyed. |
| Collision poll output | Caller | Written only for the duration of `eshi_collisions_poll()`. |
| Result, grade, and backend names | Library | Static strings; never free them. |
| `EshiMaterial` paths and uniform block | Caller | Must outlive the world or the next material binding. The core copies the struct, not the pointed-to data. |
| System callback and `user` pointer | Caller | Must remain valid until world destruction. |

No other public function returns an owned allocation. Scene nodes and render
backends are children of the world and are destroyed with it.

## Thread affinity

A world, all of its child state, and every borrowed pointer are affine to the
thread that created the world. Serialize all calls for one world on that thread.
Independent worlds may run on separate threads. User system callbacks execute
synchronously from `eshi_tick()` on the calling/world thread.

The result-string and grade-probe functions do not use a world. The OpenGL proc
loader is process-global: install it before creating Paper-grade worlds and do
not replace it while such a world is in use.

A macOS consumer may carry both Brush implementations. A source-backed
fullscreen material selects direct Metal when it is bound; a package-backed or
3D-only world selects Filament. This keeps the grade stable while allowing the
Pong reference and glTF scenes to coexist in one application binary.

On macOS, `EshiView` treats the creating Dart isolate as the render-submission
thread. Its ticker updates the world and calls the private Metal target entry
point synchronously on that isolate. Texture registration, resize,
frame-available, suspend/resume, and disposal are serialized through the
platform channel, whose handler runs on Flutter's platform thread. Flutter's
raster thread never calls the core; it only calls `copyPixelBuffer` under the
adapter's surface lock.

Each view owns its Flutter texture ID, IOSurface-backed `CVPixelBuffer`, and
borrowed `id<MTLTexture>`. Resize atomically swaps that pair. Direct Metal
borrows the texture; Filament temporarily retains the pixel buffer as an Apple
CVPixelBuffer swapchain, which is the supported zero-readback path for a BGRA
surface. The raster thread
receives an owning pixel-buffer reference, so it may finish consuming the old
surface after a resize. A synchronous Metal submission must finish before the
host announces the frame, and no world or borrowed texture handle may be used
after view/world disposal. Independent views may share a world only when their
frame submissions are serialized on that world's owner isolate.

## Assets

`EshiAsset` is an opaque handle. Loading parses and uploads once; instancing
places copies that share that upload, so a second instance costs a transform
rather than a second copy of the mesh. Both belong to the world's backend and
are released with it, so a host that exits need not release them by hand.

Geometry is a capability. A backend without a 3D scene — Ink, Paper, direct
Metal — leaves the asset entry points unimplemented and every asset call
returns `ESHI_ERR_UNSUPPORTED`. A missing or unparsable file returns
`ESHI_ERR_INVALID` after naming the file on stderr, and asking for more
instances than the asset reserved returns `ESHI_ERR_LIMIT`. A failed load
leaves the caller's handle untouched and the world renderable.

Scene conventions are glTF's own: right-handed, Y up, metres. The backend
installs a 45° vertical field of view, a photographic exposure, and one
directional light at daylight intensity. Image-based lighting is not in yet,
which is why a fully metallic material renders dark.

`eshi_asset_instance_animated` adds a root-space Y rotation rate to an
instance. The backend evaluates it while submitting the frame, so Dart declares
the spin once rather than crossing FFI with a matrix every frame. This is the
hero prototype's narrow animation seam; the retained-scene `AngularVelocity`
component in Step 8 remains the general simulation API.

## Errors and capacity

Result-bearing calls validate arguments and return an `EshiResult`; they do not
throw C++ exceptions through the C boundary. Allocation failures return
`ESHI_ERR_NOMEM`. Both shared buffers have the deterministic
`ESHI_MAX_SHARED_BUFFER_WORDS` limit and return `ESHI_ERR_LIMIT` above it.
Command decoding reports the number of whole commands applied before an error.
Event packing likewise writes only whole records and reports the number of
whole words produced.

Legacy void calls are null-safe where documented. `eshi_tick()` contains system
or allocation failures at the C boundary and drops remaining catch-up work; a
future ABI major can replace this legacy signature with a result-bearing frame
API if hosts need recovery rather than containment.

The packed-stream rules and shared golden corpus are specified in
[`WIRE_FORMAT.md`](WIRE_FORMAT.md).
