# Larimar packed command and event protocol v1

The command and event buffers are arrays of 32-bit words shared in-process
between a host and Larimar. Protocol versions are independent of the native ABI:

- `ESHI_COMMAND_PROTOCOL_VERSION == 1`
- `ESHI_EVENT_PROTOCOL_VERSION == 1`

Every record starts with one header word:

```text
31                         16 15                           0
+----------------------------+-----------------------------+
| payload word count (u16)   | opcode / event type (u16)  |
+----------------------------+-----------------------------+
```

`ESHI_CMD_HEADER`, `ESHI_CMD_OP`, and `ESHI_CMD_WORDS` are the normative word
operations. Integer payloads are unsigned numeric `u32` values. Float payloads
carry the unchanged IEEE-754 binary32 bit pattern. The in-process Dart view uses
native `Uint32List`/`Float32List` words; supported RC0 platforms are
little-endian. Canonical serialized bytes and the golden fixtures are always
little-endian, independent of how a test host stores integers.

The decoder first proves that the declared payload is inside the submitted word
count. A known v1 record may carry extra trailing payload words; a v1 decoder
uses its known prefix and ignores the extension. A too-short known record is
`ESHI_ERR_INVALID`. An unknown opcode is `ESHI_ERR_UNSUPPORTED`, never silently
skipped. This is why protocol mismatches are rejected before mapping storage.

## Command records

| Opcode | Name | Required payload | Context and effect |
|---:|---|---|---|
| 0 | `NOP` | none | Padding; valid anywhere. |
| 1 | `SCENE_BEGIN` | `u32 epoch` | Opens a retained description. Nested scenes are invalid. |
| 2 | `NODE` | `u32 key` | Selects or creates a non-zero stable key inside a scene. |
| 3 | `TRANSFORM` | `f32 x, f32 y` | Describes the selected node's transform. |
| 4 | `VELOCITY` | `f32 vx, f32 vy` | Describes its linear velocity. |
| 5 | `COLLIDER` | `f32 hx, f32 hy, u32 layer, u32 mask, u32 flags` | Describes an AABB collider. |
| 6 | `RESTITUTION` | `f32 value` | Describes collider restitution. |
| 7 | `BOUNDS` | `f32 min_x, f32 max_x, f32 min_y, f32 max_y` | Describes the motion clamp. |
| 8 | `SCENE_END` | none | Closes the scene and sweeps keys not seen in its epoch. |
| 9 | `INPUT` | `u32 EshiKey, u32 down` | Updates input outside scene topology and epoch skipping. |

A scene can span flushes. `SCENE_END` is the only operation that sweeps absent
nodes, so a buffer ending before it cannot delete unseen nodes. Epochs lower
than the highest accepted epoch are parsed but their scene operations are
ignored with `ESHI_ERR_STALE`; input still applies. Repeating the current epoch
is valid and reconciles by key without duplicating entities.

Encoders must emit a record atomically. If capacity cannot hold its header and
complete payload, they emit none of that record and return/latch
`ESHI_ERR_LIMIT`. The shared buffer is capped at
`ESHI_MAX_SHARED_BUFFER_WORDS`.

## Event records

Events use the same header framing in the reverse direction.

| Type | Name | Required payload |
|---:|---|---|
| 1 | `COLLISION` | `u32 a, u32 b, f32 nx, f32 ny, f32 penetration` |

`eshi_events_pack()` emits only complete six-word collision records. If the
next record does not fit, it returns `ESHI_ERR_LIMIT` and `out_words` describes
the complete prefix. Consumers must treat malformed or truncated records as the
end of readable input and must not dereference their payload.

## Shared golden corpus

[`commands_v1.hex`](../../core/tests/fixtures/commands_v1.hex) exercises every
command opcode in a 112-byte stream. [`events_v1.hex`](../../core/tests/fixtures/events_v1.hex)
contains one 24-byte collision. Each whitespace-separated token is one byte in
hexadecimal; both C++ and Dart tests read these source fixtures directly.

The native test encodes C++ records and compares the exact canonical bytes,
then decodes the fixtures back into world/event state. Any protocol change must
add a new versioned corpus rather than rewriting v1 in place.
