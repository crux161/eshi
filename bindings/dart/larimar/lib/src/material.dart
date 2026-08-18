part of '../larimar.dart';

/// Caller-owned uniform storage bound to a source-backed GPU material.
///
/// The view reads this block directly during render submission. The buffer is
/// invalidated when another material is bound or its world is disposed.
final class LarimarMaterialBuffer {
  LarimarMaterialBuffer._(
    this._world,
    this._sourcePath,
    this._uniforms,
    this._values,
  );

  final LarimarWorld _world;
  final ffi.Pointer<Utf8> _sourcePath;
  final ffi.Pointer<ffi.Float> _uniforms;
  final Float32List _values;
  bool _active = true;

  bool get isActive => _active && !_world.isDisposed;

  Float32List get values {
    requireActive();
    return _values;
  }

  void requireActive() {
    _world._requirePointer();
    if (!_active) {
      throw const LarimarDisposedException(
        'The material buffer was replaced by a newer binding.',
      );
    }
  }

  void _release() {
    if (!_active) return;
    _active = false;
    calloc.free(_uniforms);
    calloc.free(_sourcePath);
  }
}
