part of '../larimar.dart';

/// Base type for events drained from a Larimar world.
sealed class LarimarEvent {
  const LarimarEvent();
}

/// A collision produced by the native simulation.
final class LarimarCollisionEvent extends LarimarEvent {
  const LarimarCollisionEvent({
    required this.a,
    required this.b,
    required this.normalX,
    required this.normalY,
    required this.penetration,
  });

  final int a;
  final int b;
  final double normalX;
  final double normalY;
  final double penetration;

  @override
  bool operator ==(Object other) =>
      other is LarimarCollisionEvent &&
      a == other.a &&
      b == other.b &&
      normalX == other.normalX &&
      normalY == other.normalY &&
      penetration == other.penetration;

  @override
  int get hashCode => Object.hash(a, b, normalX, normalY, penetration);

  @override
  String toString() =>
      'LarimarCollisionEvent(a: $a, b: $b, normal: '
      '($normalX, $normalY), penetration: $penetration)';
}

/// One complete drain of the native shared event buffer.
final class LarimarEventBatch {
  const LarimarEventBatch(this.events, {required this.overflowed});

  final List<LarimarEvent> events;

  /// True when native events did not all fit and capacity should be increased.
  final bool overflowed;
}

/// Decodes whole framed records from native-endian 32-bit words.
List<LarimarEvent> decodeLarimarEvents(Uint32List words, {int? length}) {
  final wordCount = length ?? words.length;
  RangeError.checkValueInInterval(wordCount, 0, words.length, 'length');
  final events = <LarimarEvent>[];
  var cursor = 0;
  while (cursor < wordCount) {
    final header = words[cursor];
    final type = header & 0xffff;
    final payloadLength = header >> 16;
    final end = cursor + payloadLength + 1;
    if (end > wordCount) {
      throw LarimarProtocolException(
        'Truncated event record at word $cursor: expected '
        '$payloadLength payload words.',
      );
    }
    switch (type) {
      case 1:
        if (payloadLength < 5) {
          throw LarimarProtocolException(
            'Collision event at word $cursor has $payloadLength payload '
            'words; expected at least 5.',
          );
        }
        events.add(
          LarimarCollisionEvent(
            a: words[cursor + 1],
            b: words[cursor + 2],
            normalX: _bitsFloat(words[cursor + 3]),
            normalY: _bitsFloat(words[cursor + 4]),
            penetration: _bitsFloat(words[cursor + 5]),
          ),
        );
        break;
      default:
        throw LarimarProtocolException(
          'Unknown event type $type at word $cursor.',
        );
    }
    cursor = end;
  }
  return List<LarimarEvent>.unmodifiable(events);
}

double _bitsFloat(int value) {
  final bytes = ByteData(4)..setUint32(0, value, Endian.host);
  return bytes.getFloat32(0, Endian.host);
}
