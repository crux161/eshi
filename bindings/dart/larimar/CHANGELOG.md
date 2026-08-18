# Changelog

## 0.1.0-dev.2

- Add the macOS `EshiView` external-texture host using IOSurface, CoreVideo,
  and Metal without framebuffer readback.
- Add lifecycle/resize/disposal serialization, source-backed material uniforms,
  and the animated Pong reference integration.
- Add `MacOSEshiViewHost.diagnostics()` and the host-side surface accounting it
  reads, so a lifecycle loop can assert that every surface was reclaimed.
- Count the engine's surface borrows as `copies`: frames presented without
  matching borrows mean the view is not being composited.
- Add `MacOSEshiViewHost.captureSurface()`, which reads a view's surface back to
  a PNG without going through the compositor.
- Flush the Metal texture cache when a view's surface is deallocated.

## 0.1.0-dev.1

- Establish ABI v1 loading, deterministic world ownership, and command/event
  protocol v1 codecs.
