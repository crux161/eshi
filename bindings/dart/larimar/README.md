# Larimar for Dart and Flutter

This package is the FFI boundary for Larimar's native game-engine core. It
builds the OS-oblivious C++ core as a Dart code asset, checks ABI and packed
protocol compatibility before creating a world, and exposes owned worlds plus
zero-copy command and event buffers.

The package is currently consumed from this monorepo. Generated bindings are
checked in so consumers do not need LLVM; maintainers regenerate them with:

```sh
dart run ffigen --config ffigen.yaml
```

See [`docs/larimar/NATIVE_CONTRACT.md`](../../../docs/larimar/NATIVE_CONTRACT.md)
for ownership and thread-affinity rules and
[`docs/larimar/WIRE_FORMAT.md`](../../../docs/larimar/WIRE_FORMAT.md) for packed
protocol v1.

```dart
usingLarimarWorld((world) {
  world.scene()
    ..node(1)
    ..transform(x: 0, y: 0)
    ..collider(halfWidth: 0.5, halfHeight: 0.5)
    ..finish(); // one native flush for the entire description

  world.tick();
  final events = world.events().drain();
  for (final event in events.events) {
    // Handle typed LarimarCollisionEvent values here.
  }
}); // deterministic native disposal on the creating isolate
```

Use `LarimarWorld` directly when its lifetime follows a Flutter `State`; call
`dispose()` from that state's `dispose()` method. Shared command/event views are
borrowed from their world and reject use after it is disposed.
