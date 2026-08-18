part of '../larimar.dart';

const _degrees = math.pi / 180.0;

/// One pose in a declared track: where the asset is at a moment in time.
///
/// Angles are degrees because that is how a person art-directs a pose; the
/// conversion to the engine's radians happens once, here.
@immutable
final class LarimarPose {
  const LarimarPose({
    required this.at,
    this.yaw = 0,
    this.pitch = 0,
    this.scale = 1,
  });

  /// Time from the first frame the instance is drawn.
  final Duration at;

  /// Degrees about the Y axis. Keep a hero asset inside a front-facing arc:
  /// a pose track exists to present a subject, not to inspect its back.
  final double yaw;

  /// Degrees about the X axis, applied after [yaw].
  final double pitch;

  /// Uniform scale, greater than zero.
  final double scale;
}

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

  /// Places one instance that plays [keys] once and then holds the last pose.
  ///
  /// The whole track crosses the boundary in this one call and the renderer
  /// plays it, so a choreographed intro costs exactly as much per-frame FFI
  /// traffic as a still one: none. Declaring *what* the asset does stays here;
  /// advancing it stays in the core.
  void instantiateTrack({
    double x = 0,
    double y = 0,
    double z = 0,
    required List<LarimarPose> keys,
  }) {
    final world = _world;
    if (world == null) {
      throw const LarimarDisposedException('LarimarAsset has been disposed.');
    }
    if (keys.isEmpty) {
      throw ArgumentError.value(keys, 'keys', 'must hold at least one pose');
    }
    if (keys.length > native.ESHI_MAX_ASSET_KEYS) {
      throw ArgumentError.value(
        keys.length,
        'keys',
        'must not exceed ${native.ESHI_MAX_ASSET_KEYS}',
      );
    }
    for (final value in <(String, double)>[('x', x), ('y', y), ('z', z)]) {
      if (!value.$2.isFinite) {
        throw ArgumentError.value(value.$2, value.$1, 'must be finite');
      }
    }

    final buffer = calloc<native.EshiAssetKey>(keys.length);
    try {
      var previous = Duration.zero;
      for (var index = 0; index < keys.length; index += 1) {
        final pose = keys[index];
        if (pose.at < previous) {
          throw ArgumentError.value(
            pose.at,
            'keys[$index].at',
            'must not move backwards',
          );
        }
        if (!pose.scale.isFinite || pose.scale <= 0) {
          throw RangeError.value(
            pose.scale,
            'keys[$index].scale',
            'must be greater than zero',
          );
        }
        previous = pose.at;
        buffer[index]
          ..seconds = pose.at.inMicroseconds / Duration.microsecondsPerSecond
          ..yaw = pose.yaw * _degrees
          ..pitch = pose.pitch * _degrees
          ..scale = pose.scale;
      }
      _checkNative(
        native.eshi_asset_instance_track(
          world._requirePointer(),
          _handle,
          x,
          y,
          z,
          buffer,
          keys.length,
        ),
        'eshi_asset_instance_track',
      );
    } finally {
      calloc.free(buffer);
    }
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
