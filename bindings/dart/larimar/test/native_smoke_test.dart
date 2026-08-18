import 'package:larimar/larimar.dart';
import 'package:test/test.dart';

void main() {
  test('loads and validates the native contract', () {
    final contract = LarimarContract.current;
    // 1.1.0. The minor moved when the asset handles were added: additive, so a
    // package built against 1.0 still loads, which is what the check in
    // eshi_abi_check enforces.
    expect(contract.abiVersion, (1 << 24) | (1 << 16));
    expect(contract.commandProtocolVersion, 1);
    expect(contract.eventProtocolVersion, 1);
  });

  test(
    'reserves, flushes, ticks, and drains through shared native buffers',
    () {
      usingLarimarWorld((world) {
        expect(world.grade, LarimarGrade.ink);
        expect(world.backendName, isNotEmpty);

        final flush = world
            .scene(minimumWordCapacity: 64)
            .node(1)
            .transform(x: -0.25, y: 0)
            .collider(
              halfWidth: 0.5,
              halfHeight: 0.5,
              layer: 1,
              mask: 1,
              flags: LarimarColliderFlags.staticBody,
            )
            .node(2)
            .transform(x: 0.25, y: 0)
            .collider(
              halfWidth: 0.5,
              halfHeight: 0.5,
              layer: 1,
              mask: 1,
              flags: LarimarColliderFlags.staticBody,
            )
            .finish();

        expect(flush.stale, isFalse);
        expect(flush.applied, greaterThan(0));
        expect(world.sceneEpoch, 1);
        expect(world.sceneNodeCount, 2);
        expect(world.entityCount, 2);

        world.tick();
        final batch = world.events(minimumWordCapacity: 6).drain();
        expect(batch.overflowed, isFalse);
        expect(batch.events, hasLength(1));
        expect(batch.events.single, isA<LarimarCollisionEvent>());
        expect(world.frameIndex, 1);
        expect(world.simulationTime, closeTo(1 / 60, 1e-8));

        final stale = world.commands(minimumWordCapacity: 8)
          ..reset()
          ..beginScene(0)
          ..node(99)
          ..endScene();
        final staleResult = stale.flush();
        expect(staleResult.stale, isTrue);
        expect(world.sceneEpoch, 1);
        expect(world.sceneNodeCount, 2);
      });
    },
  );

  test('scene epochs advance without duplicating retained entities', () {
    usingLarimarWorld((world) {
      for (var iteration = 0; iteration < 8; iteration += 1) {
        final result = world.scene(minimumWordCapacity: 16)
          ..node(7)
          ..transform(x: iteration.toDouble(), y: 0);
        expect(result.finish().stale, isFalse);
      }
      expect(world.sceneEpoch, 8);
      expect(world.sceneNodeCount, 1);
      expect(world.entityCount, 1);
    });
  });

  test('disposal is deterministic and idempotent', () {
    for (var iteration = 0; iteration < 25; iteration += 1) {
      final world = LarimarWorld();
      final commands = world.commands(minimumWordCapacity: 8);
      world.dispose();
      world.dispose();
      expect(
        () => commands.beginScene(1),
        throwsA(isA<LarimarDisposedException>()),
      );
      expect(() => world.entityCount, throwsA(isA<LarimarDisposedException>()));
    }
  });
}
