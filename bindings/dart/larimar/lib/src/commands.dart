part of '../larimar.dart';

const int _wordMask = 0xffffffff;
const int _recordPayloadMax = 0x0000ffff;

/// Stable input keys accepted by the native command protocol.
enum LarimarKey {
  up(0),
  down(1),
  left(2),
  right(3),
  w(4),
  a(5),
  s(6),
  d(7),
  space(8),
  escape(9);

  const LarimarKey(this.value);

  final int value;
}

/// Collider flag bits used by [LarimarCommandEncoder.collider].
abstract final class LarimarColliderFlags {
  static const int none = 0;
  static const int trigger = 1 << 0;
  static const int staticBody = 1 << 1;
}

/// Encodes the versioned Larimar command stream into 32-bit words.
///
/// Records are written atomically. Once capacity is exceeded, [overflowed]
/// remains true until [reset], and no partial record is emitted.
base class LarimarCommandEncoder {
  LarimarCommandEncoder._(this._words, [this._guard]);

  /// Creates an encoder backed by ordinary Dart memory, useful for tests and
  /// offline command generation.
  factory LarimarCommandEncoder.withCapacity(int wordCapacity) {
    RangeError.checkNotNegative(wordCapacity, 'wordCapacity');
    return LarimarCommandEncoder._(Uint32List(wordCapacity));
  }

  Uint32List _words;
  final void Function()? _guard;
  int _length = 0;
  bool _overflowed = false;

  int get capacity => _words.length;
  int get length => _length;
  bool get overflowed => _overflowed;

  void reset() {
    _guard?.call();
    _length = 0;
    _overflowed = false;
  }

  LarimarCommandEncoder nop() => _record(0, const <int>[]);

  LarimarCommandEncoder beginScene(int epoch) =>
      _record(1, <int>[_requireU32(epoch, 'epoch')]);

  LarimarCommandEncoder node(int key) {
    if (key == 0) {
      throw RangeError.value(key, 'key', 'must be non-zero');
    }
    return _record(2, <int>[_requireU32(key, 'key')]);
  }

  LarimarCommandEncoder transform({required double x, required double y}) =>
      _record(3, <int>[_floatBits(x), _floatBits(y)]);

  LarimarCommandEncoder velocity({required double x, required double y}) =>
      _record(4, <int>[_floatBits(x), _floatBits(y)]);

  LarimarCommandEncoder collider({
    required double halfWidth,
    required double halfHeight,
    int layer = 1,
    int mask = _wordMask,
    int flags = LarimarColliderFlags.none,
  }) => _record(5, <int>[
    _floatBits(halfWidth),
    _floatBits(halfHeight),
    _requireU32(layer, 'layer'),
    _requireU32(mask, 'mask'),
    _requireU32(flags, 'flags'),
  ]);

  LarimarCommandEncoder restitution(double value) =>
      _record(6, <int>[_floatBits(value)]);

  LarimarCommandEncoder bounds({
    required double minimumX,
    required double maximumX,
    required double minimumY,
    required double maximumY,
  }) => _record(7, <int>[
    _floatBits(minimumX),
    _floatBits(maximumX),
    _floatBits(minimumY),
    _floatBits(maximumY),
  ]);

  LarimarCommandEncoder endScene() => _record(8, const <int>[]);

  LarimarCommandEncoder input(LarimarKey key, {required bool pressed}) =>
      _record(9, <int>[key.value, pressed ? 1 : 0]);

  Uint32List snapshotWords() {
    _guard?.call();
    return Uint32List.fromList(_words.sublist(0, _length));
  }

  /// Returns the canonical little-endian byte form used by golden fixtures.
  Uint8List canonicalBytes() {
    _guard?.call();
    final output = Uint8List(_length * 4);
    final data = ByteData.sublistView(output);
    for (var index = 0; index < _length; index += 1) {
      data.setUint32(index * 4, _words[index], Endian.little);
    }
    return output;
  }

  void _replaceStorage(Uint32List words) {
    _guard?.call();
    _words = words;
    _length = 0;
    _overflowed = false;
  }

  void _throwIfOverflowed() {
    if (_overflowed) {
      throw const LarimarBufferOverflowException(
        'The command record exceeded the reserved shared buffer.',
      );
    }
  }

  LarimarCommandEncoder _record(int opcode, List<int> payload) {
    _guard?.call();
    if (payload.length > _recordPayloadMax) {
      throw RangeError.range(payload.length, 0, _recordPayloadMax, 'payload');
    }
    final requiredWords = payload.length + 1;
    if (_overflowed || requiredWords > capacity - _length) {
      _overflowed = true;
      return this;
    }
    _words[_length] = opcode | (payload.length << 16);
    for (var index = 0; index < payload.length; index += 1) {
      _words[_length + index + 1] = payload[index] & _wordMask;
    }
    _length += requiredWords;
    return this;
  }
}

int _requireU32(int value, String name) {
  if (value < 0 || value > _wordMask) {
    throw RangeError.range(value, 0, _wordMask, name);
  }
  return value;
}

int _floatBits(double value) {
  if (!value.isFinite) {
    throw ArgumentError.value(value, 'value', 'must be finite');
  }
  final bytes = ByteData(4)..setFloat32(0, value, Endian.host);
  return bytes.getUint32(0, Endian.host);
}
