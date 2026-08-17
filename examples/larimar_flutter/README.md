# Larimar Flutter reference

The reference desktop shell for Larimar RC0. It currently loads the native
code asset, validates ABI/protocol v1, flushes one retained scene, and owns the
native world for the Flutter widget lifecycle. Plan Step 4 replaces the status
panel with the first macOS `EshiView` external texture.

```sh
flutter run -d macos
# or
flutter run -d linux
```
