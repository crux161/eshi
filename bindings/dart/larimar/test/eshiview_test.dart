import 'dart:async';

import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:larimar/larimar.dart';

void main() {
  testWidgets('serializes create, resize, lifecycle, frames, and disposal', (
    tester,
  ) async {
    final host = _FakeHost();
    final world = LarimarWorld();
    addTearDown(world.dispose);

    Future<void> mount(Size size) async {
      await tester.pumpWidget(
        MediaQuery(
          data: const MediaQueryData(devicePixelRatio: 2),
          child: Center(
            child: SizedBox(
              width: size.width,
              height: size.height,
              child: EshiView(world: world, host: host),
            ),
          ),
        ),
      );
      await tester.pump();
    }

    await mount(const Size(160, 90));
    expect(host.created, const <Size>[Size(320, 180)]);
    expect(find.byType(Texture), findsOneWidget);

    await tester.pump(const Duration(milliseconds: 16));
    expect(host.renderCount, greaterThan(0));
    expect(host.presentCount, greaterThan(0));

    await mount(const Size(200, 120));
    expect(host.resized, const <Size>[Size(400, 240)]);

    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.paused);
    await tester.pump();
    expect(host.suspensions.last, isTrue);

    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
    await tester.pump();
    expect(host.suspensions.last, isFalse);

    await tester.pumpWidget(const SizedBox());
    await tester.pump();
    expect(host.disposed, 1);
  });

  testWidgets('disposes a surface created after the widget is removed', (
    tester,
  ) async {
    final host = _FakeHost()..delayCreate = true;
    final world = LarimarWorld();
    addTearDown(world.dispose);

    await tester.pumpWidget(
      SizedBox(
        width: 64,
        height: 64,
        child: EshiView(world: world, host: host),
      ),
    );
    await tester.pump();
    await tester.pumpWidget(const SizedBox());
    host.finishCreate();
    await tester.pump();
    await tester.pump();

    expect(host.disposed, 1);
  });
}

final class _FakeHost implements EshiViewHost {
  final List<Size> created = <Size>[];
  final List<Size> resized = <Size>[];
  final List<bool> suspensions = <bool>[];
  bool delayCreate = false;
  int renderCount = 0;
  int presentCount = 0;
  int disposed = 0;
  int _nextTexture = 1;
  EshiViewSurface? _pendingSurface;
  Completer<EshiViewSurface>? _pendingCreate;

  @override
  Future<EshiViewSurface> create({required int width, required int height}) {
    created.add(Size(width.toDouble(), height.toDouble()));
    final surface = _surface(width, height);
    if (!delayCreate) return Future<EshiViewSurface>.value(surface);
    _pendingSurface = surface;
    _pendingCreate = Completer<EshiViewSurface>();
    return _pendingCreate!.future;
  }

  void finishCreate() => _pendingCreate!.complete(_pendingSurface!);

  @override
  Future<void> dispose(EshiViewSurface surface) async {
    disposed += 1;
  }

  @override
  Future<void> present(EshiViewSurface surface) async {
    presentCount += 1;
  }

  @override
  void render(EshiViewSurface surface, LarimarWorld world, double time) {
    renderCount += 1;
  }

  @override
  Future<EshiViewSurface> resize(
    EshiViewSurface surface, {
    required int width,
    required int height,
  }) async {
    resized.add(Size(width.toDouble(), height.toDouble()));
    return EshiViewSurface(
      textureId: surface.textureId,
      metalTexture: surface.metalTexture + 1,
      width: width,
      height: height,
    );
  }

  @override
  Future<void> setSuspended(EshiViewSurface surface, bool suspended) async {
    suspensions.add(suspended);
  }

  EshiViewSurface _surface(int width, int height) => EshiViewSurface(
    textureId: _nextTexture++,
    metalTexture: 100,
    width: width,
    height: height,
  );
}
