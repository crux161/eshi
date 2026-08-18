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

On macOS, a Brush-grade world can render through `EshiView`. The widget owns an
IOSurface-backed Flutter texture, serializes resize and application lifecycle
operations, and submits Metal directly without copying a framebuffer through
the CPU:

```dart
EshiView(
  world: world,
  onFrame: (world, elapsed, delta) {
    world.tick(delta.inMicroseconds / Duration.microsecondsPerSecond);
  },
)
```

Bind a source-backed material with `world.bindShaderMaterial(...)` before the
first frame. Its returned `LarimarMaterialBuffer` remains world-owned and is
invalidated when the material is replaced or the world is disposed.

`MacOSEshiViewHost.diagnostics()` reports the host's surface accounting —
surfaces created, resized, presented, disposed, still registered, and still
alive. `EshiView` never calls it; it exists so a lifecycle test can prove the
host released what it allocated. Both halves of that gate run from the
repository root:

```sh
./scripts/check_eshiview_host.sh && ./scripts/check_eshiview_lifecycle.sh
```

The first builds the adapter twice — once under `leaks --atExit`, once under
ThreadSanitizer — and drives it from a platform thread and a stand-in raster
queue with no engine present. The second runs the reference application's real
create/resize/background/foreground/destroy loop and snapshots the process from
outside the App Sandbox. Both write their reports, and the composited frames the
visual test captured, to `build/larimar-diagnostics/`.
