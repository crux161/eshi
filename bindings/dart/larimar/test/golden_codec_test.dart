import 'dart:io';
import 'dart:typed_data';

import 'package:larimar/larimar.dart';
import 'package:test/test.dart';

void main() {
  group('command protocol v1', () {
    test('matches the language-neutral C++ golden fixture', () {
      final encoder = LarimarCommandEncoder.withCapacity(64)
        ..nop()
        ..beginScene(42)
        ..node(7)
        ..transform(x: 1.5, y: -2.25)
        ..velocity(x: 0.25, y: -0.5)
        ..collider(
          halfWidth: 0.5,
          halfHeight: 0.25,
          layer: 1,
          mask: 2,
          flags: LarimarColliderFlags.staticBody,
        )
        ..restitution(0.75)
        ..bounds(minimumX: -3, maximumX: 3, minimumY: -4, maximumY: 4)
        ..endScene()
        ..input(LarimarKey.space, pressed: true);

      expect(encoder.overflowed, isFalse);
      expect(
        encoder.canonicalBytes(),
        orderedEquals(_fixtureBytes('commands_v1.hex')),
      );
    });

    test('writes records atomically and latches overflow', () {
      final encoder = LarimarCommandEncoder.withCapacity(2)..beginScene(1);
      final before = encoder.snapshotWords();
      encoder.node(7);

      expect(encoder.overflowed, isTrue);
      expect(encoder.length, 2);
      expect(encoder.snapshotWords(), orderedEquals(before));
      expect(encoder.node(8).length, 2);
      expect(encoder.snapshotWords(), orderedEquals(before));

      encoder.reset();
      expect(encoder.length, 0);
      expect(encoder.overflowed, isFalse);
    });
  });

  group('event protocol v1', () {
    test('decodes the language-neutral C++ golden fixture', () {
      final events = decodeLarimarEvents(
        _littleEndianWords(_fixtureBytes('events_v1.hex')),
      );

      expect(events, const <LarimarEvent>[
        LarimarCollisionEvent(
          a: 1,
          b: 2,
          normalX: -1,
          normalY: 0,
          penetration: 0.5,
        ),
      ]);
    });

    test('rejects truncated and unknown records', () {
      expect(
        () => decodeLarimarEvents(Uint32List.fromList(<int>[5 << 16 | 1, 1])),
        throwsA(isA<LarimarProtocolException>()),
      );
      expect(
        () => decodeLarimarEvents(Uint32List.fromList(<int>[99])),
        throwsA(isA<LarimarProtocolException>()),
      );
    });

    test('accepts a known prefix extended by a compatible writer', () {
      final words = Uint32List.fromList(<int>[
        6 << 16 | 1,
        1,
        2,
        0xbf800000,
        0,
        0x3f000000,
        0xdecafbad,
      ]);
      final event = decodeLarimarEvents(words).single as LarimarCollisionEvent;
      expect(event.a, 1);
      expect(event.penetration, 0.5);
    });
  });
}

Uint8List _fixtureBytes(String name) {
  final text = File('../../../core/tests/fixtures/$name').readAsStringSync();
  final tokens = text.trim().split(RegExp(r'\s+'));
  return Uint8List.fromList(
    tokens.map((token) => int.parse(token, radix: 16)).toList(),
  );
}

Uint32List _littleEndianWords(Uint8List bytes) {
  expect(bytes.length % 4, 0);
  final data = ByteData.sublistView(bytes);
  return Uint32List.fromList(<int>[
    for (var offset = 0; offset < bytes.length; offset += 4)
      data.getUint32(offset, Endian.little),
  ]);
}
