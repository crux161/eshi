part of '../larimar.dart';

/// Larimar's capability grades, ordered from CPU-only to specialized hardware.
enum LarimarGrade {
  ink(1),
  paper(2),
  brush(3),
  gold(4);

  const LarimarGrade(this.value);

  final int value;
}

/// Immutable options used to construct a [LarimarWorld].
final class LarimarWorldConfig {
  const LarimarWorldConfig({
    this.width = 960,
    this.height = 540,
    this.grade = LarimarGrade.ink,
    this.fixedDelta = 1 / 60,
    this.seed = 0x5eed5eed,
    this.maxEntities = 4096,
  });

  final int width;
  final int height;
  final LarimarGrade grade;
  final double fixedDelta;
  final int seed;
  final int maxEntities;
}

/// Outcome of one shared command-buffer flush.
final class LarimarFlushResult {
  const LarimarFlushResult({required this.applied, required this.stale});

  final int applied;
  final bool stale;
}

/// Owns one native world and every shared view borrowed from it.
///
/// Worlds are isolate-affine and must be disposed explicitly on their creating
/// isolate. Disposal is idempotent. Larimar intentionally does not attach a
/// finalizer because a finalizer cannot preserve native thread affinity.
final class LarimarWorld {
  factory LarimarWorld({
    LarimarWorldConfig config = const LarimarWorldConfig(),
  }) {
    LarimarContract.current;
    _validateConfig(config);
    final nativeGrade = native.EshiGrade.fromValue(config.grade.value);
    if (native.eshi_grade_available(nativeGrade) == 0) {
      throw LarimarNativeException(
        LarimarResult.unsupported,
        '${config.grade.name} is not available in this native library or host.',
      );
    }
    final pointer = using((arena) {
      final nativeConfig = arena<native.EshiConfig>();
      nativeConfig.ref
        ..width = config.width
        ..height = config.height
        ..gradeAsInt = config.grade.value
        ..fixed_dt = config.fixedDelta
        ..seed = config.seed
        ..max_entities = config.maxEntities;
      return native.eshi_world_create(nativeConfig);
    });
    if (pointer == ffi.nullptr) {
      throw const LarimarNativeException(
        LarimarResult.outOfMemory,
        'eshi_world_create returned null; the requested grade may be '
        'unavailable or allocation failed.',
      );
    }
    return LarimarWorld._(pointer, Isolate.current);
  }

  LarimarWorld._(this._pointer, this._ownerIsolate);

  ffi.Pointer<native.EshiWorld> _pointer;
  final Isolate _ownerIsolate;
  LarimarCommandBuffer? _commandBuffer;
  LarimarEventBuffer? _eventBuffer;
  LarimarMaterialBuffer? _materialBuffer;
  int _nextEpoch = 1;
  bool _disposed = false;

  bool get isDisposed => _disposed;

  int get entityCount => native.eshi_entity_count(_requirePointer());
  int get sceneEpoch => native.eshi_scene_epoch(_requirePointer());
  int get sceneNodeCount => native.eshi_scene_node_count(_requirePointer());
  int get frameIndex => native.eshi_frame_index(_requirePointer());
  double get simulationTime => native.eshi_sim_time(_requirePointer());

  LarimarGrade get grade {
    final value = native.eshi_world_grade(_requirePointer()).value;
    return LarimarGrade.values.singleWhere((grade) => grade.value == value);
  }

  String get backendName {
    final value = native.eshi_world_backend_name(_requirePointer());
    return value == ffi.nullptr ? 'unknown' : value.cast<Utf8>().toDartString();
  }

  /// Maps the native command buffer, growing it only when required.
  LarimarCommandBuffer commands({int minimumWordCapacity = 1024}) {
    _requirePointer();
    RangeError.checkNotNegative(minimumWordCapacity, 'minimumWordCapacity');
    final buffer = _commandBuffer ??= LarimarCommandBuffer._(this);
    buffer.reserve(minimumWordCapacity);
    return buffer;
  }

  /// Starts a retained scene description with a monotonically increasing epoch.
  LarimarSceneWriter scene({int minimumWordCapacity = 1024}) {
    final currentEpoch = sceneEpoch;
    if (_nextEpoch <= currentEpoch) {
      _nextEpoch = currentEpoch + 1;
    }
    final buffer = commands(minimumWordCapacity: minimumWordCapacity)..reset();
    return LarimarSceneWriter._(buffer, _nextEpoch++);
  }

  /// Maps the native event buffer, growing it only when required.
  LarimarEventBuffer events({int minimumWordCapacity = 256}) {
    _requirePointer();
    RangeError.checkNotNegative(minimumWordCapacity, 'minimumWordCapacity');
    final buffer = _eventBuffer ??= LarimarEventBuffer._(this);
    buffer.reserve(minimumWordCapacity);
    return buffer;
  }

  void tick([double delta = 1 / 60]) {
    if (!delta.isFinite || delta < 0) {
      throw ArgumentError.value(
        delta,
        'delta',
        'must be finite and non-negative',
      );
    }
    native.eshi_tick(_requirePointer(), delta);
  }

  void clearScene() => native.eshi_scene_clear(_requirePointer());

  /// Binds a runtime-transpiled GPU material and returns its live uniform block.
  ///
  /// [sourcePath] must remain readable when this call is made. Larimar copies
  /// the path and owns the uniform allocation until this world is disposed or
  /// another material replaces it.
  LarimarMaterialBuffer bindShaderMaterial({
    required String sourcePath,
    required int uniformFloatCount,
  }) {
    final pointer = _requirePointer();
    if (sourcePath.isEmpty) {
      throw ArgumentError.value(sourcePath, 'sourcePath', 'must not be empty');
    }
    RangeError.checkValueInInterval(
      uniformFloatCount,
      0,
      64,
      'uniformFloatCount',
    );
    final source = sourcePath.toNativeUtf8();
    final allocationCount = uniformFloatCount == 0 ? 1 : uniformFloatCount;
    final uniforms = calloc<ffi.Float>(allocationCount);
    final buffer = LarimarMaterialBuffer._(
      this,
      source,
      uniforms,
      uniforms.asTypedList(uniformFloatCount),
    );
    try {
      using((arena) {
        final material = arena<native.EshiMaterial>();
        material.ref
          ..cpu_shader = ffi.nullptr
              .cast<ffi.NativeFunction<native.EshiShaderFnFunction>>()
          ..source_path = source.cast()
          ..package_path = ffi.nullptr
          ..uniform_data = uniforms.cast()
          ..uniform_size = uniformFloatCount * ffi.sizeOf<ffi.Float>();
        _checkNative(
          native.eshi_material_set(pointer, material),
          'eshi_material_set',
        );
      });
    } catch (_) {
      buffer._release();
      rethrow;
    }
    _materialBuffer?._release();
    _materialBuffer = buffer;
    return buffer;
  }

  /// Resolves the current entity handle for a retained scene key.
  int sceneEntity(int key) {
    _requireU32(key, 'key');
    if (key == 0) throw RangeError.value(key, 'key', 'must be non-zero');
    final entity = native.eshi_scene_entity(_requirePointer(), key);
    if (entity == 0) {
      throw StateError('No live scene entity is bound to key $key.');
    }
    return entity;
  }

  ({double x, double y}) transformOf(int entity) => using((arena) {
    final x = arena<ffi.Float>();
    final y = arena<ffi.Float>();
    _checkNative(
      native.eshi_transform_get(_requirePointer(), entity, x, y),
      'eshi_transform_get',
    );
    return (x: x.value, y: y.value);
  });

  ({double x, double y}) velocityOf(int entity) => using((arena) {
    final x = arena<ffi.Float>();
    final y = arena<ffi.Float>();
    _checkNative(
      native.eshi_velocity_get(_requirePointer(), entity, x, y),
      'eshi_velocity_get',
    );
    return (x: x.value, y: y.value);
  });

  void setTransform(int entity, {required double x, required double y}) {
    _checkNative(
      native.eshi_transform_set(_requirePointer(), entity, x, y),
      'eshi_transform_set',
    );
  }

  void setVelocity(int entity, {required double x, required double y}) {
    _checkNative(
      native.eshi_velocity_set(_requirePointer(), entity, x, y),
      'eshi_velocity_set',
    );
  }

  /// Releases native state. Repeating the call on the owner isolate is safe.
  void dispose() {
    _requireOwnerIsolate();
    if (_disposed) {
      return;
    }
    native.eshi_world_destroy(_pointer);
    _materialBuffer?._release();
    _materialBuffer = null;
    _pointer = ffi.nullptr;
    _disposed = true;
  }

  ffi.Pointer<native.EshiWorld> _requirePointer() {
    _requireOwnerIsolate();
    if (_disposed) {
      throw const LarimarDisposedException('LarimarWorld has been disposed.');
    }
    return _pointer;
  }

  void _requireOwnerIsolate() {
    if (Isolate.current != _ownerIsolate) {
      throw const LarimarIsolateException(
        'LarimarWorld must be used and disposed on its creating isolate.',
      );
    }
  }
}

/// Runs [body] and deterministically disposes its world afterward.
T usingLarimarWorld<T>(
  T Function(LarimarWorld world) body, {
  LarimarWorldConfig config = const LarimarWorldConfig(),
}) {
  final world = LarimarWorld(config: config);
  try {
    return body(world);
  } finally {
    world.dispose();
  }
}

/// A zero-copy view over a world's native command storage.
final class LarimarCommandBuffer extends LarimarCommandEncoder {
  LarimarCommandBuffer._(this._world)
    : super._(Uint32List(0), _world._requirePointer);

  final LarimarWorld _world;

  void reserve(int minimumWordCapacity) {
    final pointer = _world._requirePointer();
    final requested = minimumWordCapacity == 0 ? 1 : minimumWordCapacity;
    _requireU32(requested, 'minimumWordCapacity');
    if (native.eshi_commands_capacity(pointer) < requested) {
      _checkNative(
        native.eshi_commands_reserve(pointer, requested),
        'eshi_commands_reserve',
      );
    }
    final capacity = native.eshi_commands_capacity(pointer);
    final data = native.eshi_commands_data(pointer);
    if (data == ffi.nullptr || capacity == 0) {
      throw const LarimarNativeException(
        LarimarResult.outOfMemory,
        'eshi_commands_data returned null after a successful reserve.',
      );
    }
    if (capacity != this.capacity) {
      _replaceStorage(data.asTypedList(capacity));
    }
  }

  /// Crosses FFI once to apply every encoded record.
  LarimarFlushResult flush({bool resetAfter = true}) {
    final pointer = _world._requirePointer();
    _throwIfOverflowed();
    try {
      return using((arena) {
        final applied = arena<ffi.Uint32>();
        final result = native.eshi_commands_flush(pointer, length, applied);
        final stale = result.value == LarimarResult.stale.code;
        if (!stale) {
          _checkNative(result, 'eshi_commands_flush');
        }
        return LarimarFlushResult(applied: applied.value, stale: stale);
      });
    } finally {
      if (resetAfter) {
        reset();
      }
    }
  }
}

/// Fluent retained-scene writer. Call [finish] exactly once.
final class LarimarSceneWriter {
  LarimarSceneWriter._(this._buffer, this.epoch) {
    _buffer.beginScene(epoch);
  }

  final LarimarCommandBuffer _buffer;
  final int epoch;
  bool _finished = false;

  LarimarSceneWriter node(int key) {
    _requireOpen();
    _buffer.node(key);
    return this;
  }

  LarimarSceneWriter transform({required double x, required double y}) {
    _requireOpen();
    _buffer.transform(x: x, y: y);
    return this;
  }

  LarimarSceneWriter velocity({required double x, required double y}) {
    _requireOpen();
    _buffer.velocity(x: x, y: y);
    return this;
  }

  LarimarSceneWriter collider({
    required double halfWidth,
    required double halfHeight,
    int layer = 1,
    int mask = _wordMask,
    int flags = LarimarColliderFlags.none,
  }) {
    _requireOpen();
    _buffer.collider(
      halfWidth: halfWidth,
      halfHeight: halfHeight,
      layer: layer,
      mask: mask,
      flags: flags,
    );
    return this;
  }

  LarimarSceneWriter restitution(double value) {
    _requireOpen();
    _buffer.restitution(value);
    return this;
  }

  LarimarSceneWriter bounds({
    required double minimumX,
    required double maximumX,
    required double minimumY,
    required double maximumY,
  }) {
    _requireOpen();
    _buffer.bounds(
      minimumX: minimumX,
      maximumX: maximumX,
      minimumY: minimumY,
      maximumY: maximumY,
    );
    return this;
  }

  LarimarFlushResult finish() {
    _requireOpen();
    _finished = true;
    _buffer.endScene();
    return _buffer.flush();
  }

  void _requireOpen() {
    if (_finished) {
      throw StateError('LarimarSceneWriter has already finished.');
    }
  }
}

/// A zero-copy view over packed native events.
final class LarimarEventBuffer {
  LarimarEventBuffer._(this._world);

  final LarimarWorld _world;
  Uint32List _words = Uint32List(0);

  int get capacity => _words.length;

  void reserve(int minimumWordCapacity) {
    final pointer = _world._requirePointer();
    final requested = minimumWordCapacity == 0 ? 1 : minimumWordCapacity;
    _requireU32(requested, 'minimumWordCapacity');
    if (native.eshi_events_capacity(pointer) < requested) {
      _checkNative(
        native.eshi_events_reserve(pointer, requested),
        'eshi_events_reserve',
      );
    }
    final capacity = native.eshi_events_capacity(pointer);
    final data = native.eshi_events_data(pointer);
    if (data == ffi.nullptr || capacity == 0) {
      throw const LarimarNativeException(
        LarimarResult.outOfMemory,
        'eshi_events_data returned null after a successful reserve.',
      );
    }
    if (capacity != _words.length) {
      _words = data.asTypedList(capacity);
    }
  }

  /// Packs and decodes all whole events that fit in the shared storage.
  LarimarEventBatch drain() {
    final pointer = _world._requirePointer();
    if (_words.isEmpty) {
      reserve(256);
    }
    return using((arena) {
      final written = arena<ffi.Uint32>();
      final result = native.eshi_events_pack(pointer, written);
      final overflowed = result.value == LarimarResult.limit.code;
      if (!overflowed) {
        _checkNative(result, 'eshi_events_pack');
      }
      return LarimarEventBatch(
        decodeLarimarEvents(_words, length: written.value),
        overflowed: overflowed,
      );
    });
  }
}

void _validateConfig(LarimarWorldConfig config) {
  RangeError.checkValueInInterval(config.width, 1, 0x7fffffff, 'width');
  RangeError.checkValueInInterval(config.height, 1, 0x7fffffff, 'height');
  if (!config.fixedDelta.isFinite || config.fixedDelta < 0) {
    throw ArgumentError.value(
      config.fixedDelta,
      'fixedDelta',
      'must be finite and non-negative',
    );
  }
  RangeError.checkNotNegative(config.seed, 'seed');
  RangeError.checkValueInInterval(
    config.maxEntities,
    0,
    native.ESHI_MAX_ENTITIES,
    'maxEntities',
  );
}
