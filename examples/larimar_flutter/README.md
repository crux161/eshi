# Larimar Flutter reference

The reference desktop shell for Larimar RC0. On macOS it renders Pong through an
`EshiView` external texture — an IOSurface-backed Metal target the Brush backend
writes into directly — with ordinary Flutter UI composited above it. On Linux it
loads the native code asset, validates ABI/protocol v1, flushes one retained
scene, and owns the native world for the Flutter widget lifecycle.

The brand hero is the first real glTF scene to use the same view. Filament loads
`resources/larimar-model.glb`, the native renderer owns its slow Y-axis spin,
and Flutter layers the HarmonyOS Sans wordmark and a 4.54-second, soundtrack-
shaped color transition around its transparent texture. Audio remains host UI,
not an engine subsystem.

```sh
flutter run -d macos -t lib/hero.dart
```

```sh
flutter run -d macos
# or
flutter run -d linux
```

`integration_test/eshiview_lifecycle_test.dart` is the Step 4 gate: a
create/resize/background/foreground/destroy loop that asserts the host reclaimed
every surface, and a visual test that captures the composited frames. Run it
through the harness, which supplies the cycle count and snapshots the process
from outside the App Sandbox:

```sh
../../scripts/check_eshiview_lifecycle.sh
```
