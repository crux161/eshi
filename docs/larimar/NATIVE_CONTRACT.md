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
not replace it while such a world is in use. The future Flutter host must marshal
view resize, render, and disposal to its render thread; it must not call through
a stale Dart isolate after disposal.

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
