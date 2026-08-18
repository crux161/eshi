part of '../larimar.dart';

/// One glTF/GLB upload owned by a [LarimarWorld].
///
/// Geometry and textures are uploaded once. Calling [instantiate] only adds a
/// transform and renderable entities, so multiple instances remain cheap.
final class LarimarAsset {
  LarimarAsset._(this._world, this._handle);

  LarimarWorld? _world;
  final int _handle;

  bool get isDisposed => _world == null;

  /// Places one instance in glTF's right-handed, Y-up coordinate system.
  ///
  /// [spinY] is integrated inside the native renderer. Declaring a non-zero
  /// rate therefore does not add per-frame Dart-to-native traffic.
  void instantiate({
    double x = 0,
    double y = 0,
    double z = 0,
    double scale = 1,
    double spinY = 0,
  }) {
    final world = _world;
    if (world == null) {
      throw const LarimarDisposedException('LarimarAsset has been disposed.');
    }
    for (final value in <(String, double)>[
      ('x', x),
      ('y', y),
      ('z', z),
      ('scale', scale),
      ('spinY', spinY),
    ]) {
      if (!value.$2.isFinite) {
        throw ArgumentError.value(value.$2, value.$1, 'must be finite');
      }
    }
    if (scale <= 0) {
      throw RangeError.value(scale, 'scale', 'must be greater than zero');
    }
    _checkNative(
      native.eshi_asset_instance_animated(
        world._requirePointer(),
        _handle,
        x,
        y,
        z,
        scale,
        spinY,
      ),
      'eshi_asset_instance_animated',
    );
  }

  /// Releases the upload and all of its instances. Idempotent.
  void dispose() {
    final world = _world;
    if (world == null) return;
    world._releaseAsset(this);
    _world = null;
  }

  void _worldDisposed() => _world = null;
}
