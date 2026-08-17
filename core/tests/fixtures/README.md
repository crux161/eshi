# Larimar wire golden corpus

These files are the language-neutral byte fixtures for packed protocol v1.
Each whitespace-separated token is one byte in hexadecimal. C++ tests and the
Step 3 Dart codec tests consume these exact files; neither side owns a generated
copy.

- `commands_v1.hex` exercises every v1 command in one retained-scene stream.
- `events_v1.hex` is one collision event: entities 1 and 2, normal (-1, 0),
  penetration 0.5.

Multi-byte values are little-endian as specified in
[`WIRE_FORMAT.md`](../../../docs/larimar/WIRE_FORMAT.md).
