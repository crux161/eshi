// The Step 4 gate: the macOS EshiView lifecycle under leak diagnostics, and a
// visual smoke test that keeps the evidence.
//
// Run it through scripts/check_eshiview_lifecycle.sh rather than directly. The
// script supplies the cycle count, snapshots `leaks` and `vmmap` from outside
// the App Sandbox — a sandboxed process cannot examine itself — and copies the
// captured frames out of the app container.
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:larimar/larimar.dart';
import 'package:larimar_flutter/main.dart';

/// Create/resize/background/foreground/destroy rounds run by the leak gate.
const _cycles = int.fromEnvironment(
  'LARIMAR_LIFECYCLE_CYCLES',
  defaultValue: 5,
);

/// True when an external harness is listening for snapshot requests.
const _leakHarness = bool.fromEnvironment('LARIMAR_LEAK_HARNESS');

const _host = MacOSEshiViewHost();

const _viewSize = Size(640, 360);

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('reclaims every surface across $_cycles lifecycle cycles', (
    tester,
  ) async {
    // The App Sandbox rewrites the temp directory into the app container, and
    // the harness outside cannot guess that path. Announce it before anything
    // else so snapshot requests and captures can be found.
    debugPrint('larimar-tmp: ${Directory.systemTemp.path}');
    if (!_brushIsAvailable()) return;

    final start = await _host.diagnostics();
    expect(start.metalAvailable, isTrue);
    expect(start.registered, 0);

    for (var cycle = 0; cycle < _cycles; cycle += 1) {
      final key = ValueKey<int>(cycle);
      await tester.pumpWidget(LarimarApp(key: key, testViewSize: _viewSize));
      await _pumpUntilTexture(tester);
      expect(find.byKey(const ValueKey<String>('larimar-error')), findsNothing);

      await tester.pumpWidget(
        LarimarApp(
          key: key,
          testViewSize: Size(
            _viewSize.width + (cycle % 4 + 1) * 8,
            _viewSize.height + (cycle % 4 + 1) * 8,
          ),
        ),
      );
      await tester.pump(const Duration(milliseconds: 100));
      expect(find.byType(Texture), findsOneWidget);

      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.paused);
      await tester.pump(const Duration(milliseconds: 50));
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
      await tester.pump(const Duration(milliseconds: 100));
      expect(find.byType(Texture), findsOneWidget);

      await tester.pumpWidget(const SizedBox());
      await tester.pump(const Duration(milliseconds: 100));
      expect(find.byType(Texture), findsNothing);

      final settled = await _settle(tester);
      expect(
        settled.registered,
        0,
        reason: 'cycle $cycle left a surface registered with the host',
      );
      expect(
        settled.live,
        0,
        reason: 'cycle $cycle left a surface alive after disposal',
      );

      // One cycle is enough to charge every one-time allocation — the Metal
      // device, the shader pipeline, the texture cache — so the baseline is
      // taken here rather than before the loop. Everything after it must be
      // reclaimed, and that is exactly what the harness compares.
      if (cycle == 0) await _requestSnapshot('baseline');
    }

    final end = await _settle(tester);
    debugPrint('larimar-diagnostics: $end');
    expect(end.created, _cycles);
    expect(end.disposed, end.created);
    expect(end.resized, greaterThanOrEqualTo(_cycles));
    expect(end.presented, greaterThan(0));
    expect(end.suspensions, greaterThanOrEqualTo(_cycles * 2));
    expect(end.registered, 0);
    expect(end.live, 0);
    expect(end.liveSurfaceBytes, 0);

    await _requestSnapshot('final');
  });

  testWidgets('draws animated Pong into the surface Flutter composites', (
    tester,
  ) async {
    if (!_brushIsAvailable()) return;

    await tester.pumpWidget(const LarimarApp(testViewSize: _viewSize));
    await _pumpUntilTexture(tester);
    expect(find.text('Flutter UI over animated Pong'), findsOneWidget);
    expect(find.byKey(const ValueKey<String>('larimar-error')), findsNothing);

    // Two separate claims, and they need two separate instruments.
    //
    // The first is that Larimar draws the frame: read back the surface itself,
    // which is the memory the engine was handed. The second is that the engine
    // composites it, which only its borrow count can answer.
    //
    // What is deliberately *not* used here is RepaintBoundary.toImage over the
    // app: it rasterizes the layer tree offscreen and does not reliably include
    // an external texture layer. Measured on this engine, it returned the
    // Flutter UI over an empty view while the surface held a complete frame and
    // the engine was borrowing it 60 times a second. A capture that can come
    // back blank when everything works is not evidence of anything.
    final drawn = await _pumpUntilComposited(tester, 4);
    expect(
      drawn.copies,
      greaterThan(0),
      reason: 'the engine never borrowed the surface; the view is a hole',
    );

    final surface = _textureSurface(tester);
    final first = await _captureSurface(surface, 'eshiview-pong-first.png');
    await _pumpUntilComposited(tester, 20);
    final second = await _captureSurface(surface, 'eshiview-pong-second.png');

    // Pong's ball and paddles are the brightest things in the frame, so peak
    // luminance separates a drawn frame from a cleared one.
    expect(
      first.peakLuminance(),
      greaterThan(0.5),
      reason: 'the surface is dark everywhere; no frame reached it',
    );
    expect(
      first.changedFraction(second),
      greaterThan(0.002),
      reason: 'two captures 20 frames apart are identical; the scene is frozen',
    );
  });
}

/// A Brush-grade world needs a Metal device. Virtualized runners have none, and
/// that is a skip, not a failure — the same rule the Metal CI job follows.
bool _brushIsAvailable() {
  try {
    LarimarWorld(
      config: const LarimarWorldConfig(grade: LarimarGrade.brush),
    ).dispose();
    return true;
  } on LarimarNativeException catch (error) {
    if (error.result == LarimarResult.unsupported) {
      debugPrint('larimar-skip: no Metal device on this host');
      return false;
    }
    rethrow;
  }
}

Future<void> _pumpUntilTexture(WidgetTester tester) async {
  for (var attempt = 0; attempt < 60; attempt += 1) {
    await tester.pump(const Duration(milliseconds: 50));
    if (find.byType(Texture).evaluate().isNotEmpty) return;
  }
  fail('the EshiView texture never appeared');
}

/// Pumps until the engine has composited [frames] more frames of the view.
///
/// One `tester.pump()` is one frame here, so a single long pump advances the
/// scene by one step no matter how long it waits. Counting the host's borrows
/// is both the honest measure of progress and the one that cannot pass while
/// nothing is being drawn.
Future<EshiViewDiagnostics> _pumpUntilComposited(
  WidgetTester tester,
  int frames,
) async {
  var diagnostics = await _host.diagnostics();
  final target = diagnostics.copies + frames;
  for (
    var attempt = 0;
    attempt < 240 && diagnostics.copies < target;
    attempt += 1
  ) {
    await tester.pump(const Duration(milliseconds: 16));
    diagnostics = await _host.diagnostics();
  }
  if (diagnostics.copies < target) {
    fail(
      'the engine composited ${diagnostics.copies} frames, expected $target; '
      'the view is not reaching the screen',
    );
  }
  return diagnostics;
}

/// Reads the host's accounting once the raster thread has let go of everything
/// the platform thread already released.
Future<EshiViewDiagnostics> _settle(WidgetTester tester) async {
  var diagnostics = await _host.diagnostics();
  for (var attempt = 0; attempt < 40 && diagnostics.live > 0; attempt += 1) {
    await tester.pump(const Duration(milliseconds: 50));
    diagnostics = await _host.diagnostics();
  }
  return diagnostics;
}

/// Asks the external harness for a `leaks`/`vmmap` snapshot and waits for it.
Future<void> _requestSnapshot(String phase) async {
  if (!_leakHarness) return;
  final request = File(
    '${Directory.systemTemp.path}/larimar-lifecycle.request',
  );
  final ack = File('${Directory.systemTemp.path}/larimar-lifecycle.ack');
  if (ack.existsSync()) ack.deleteSync();
  request.writeAsStringSync(phase, flush: true);
  final deadline = DateTime.now().add(const Duration(seconds: 180));
  while (DateTime.now().isBefore(deadline)) {
    if (ack.existsSync() && ack.readAsStringSync().trim() == phase) return;
    await Future<void>.delayed(const Duration(milliseconds: 100));
  }
  fail('the leak harness never acknowledged the "$phase" snapshot');
}

/// The surface behind the mounted view, addressed by the id Flutter is using.
EshiViewSurface _textureSurface(WidgetTester tester) => EshiViewSurface(
  textureId: tester.widget<Texture>(find.byType(Texture)).textureId,
  metalTexture: 0,
  width: 0,
  height: 0,
);

Future<_Frame> _captureSurface(EshiViewSurface surface, String name) async {
  final file = File('${Directory.systemTemp.path}/$name');
  await _host.captureSurface(surface, file.path);
  // The harness copies anything announced this way out of the app container.
  debugPrint('larimar-artifact: ${file.path}');

  final image = await decodeImageFromList(await file.readAsBytes());
  try {
    final raw = await image.toByteData();
    return _Frame(
      width: image.width,
      height: image.height,
      pixels: raw!.buffer.asUint8List(),
    );
  } finally {
    image.dispose();
  }
}

/// One read-back of the view's surface: what Larimar drew, before compositing.
final class _Frame {
  const _Frame({
    required this.width,
    required this.height,
    required this.pixels,
  });

  final int width;
  final int height;
  final Uint8List pixels;

  double peakLuminance() {
    var peak = 0.0;
    for (var offset = 0; offset + 2 < pixels.length; offset += 4) {
      final luminance =
          (pixels[offset] + pixels[offset + 1] + pixels[offset + 2]) /
          (3 * 255);
      if (luminance > peak) peak = luminance;
    }
    return peak;
  }

  double changedFraction(_Frame other) {
    if (other.pixels.length != pixels.length) return 1;
    var counted = 0;
    var changed = 0;
    for (var offset = 0; offset + 2 < pixels.length; offset += 4) {
      counted += 1;
      final delta =
          (pixels[offset] - other.pixels[offset]).abs() +
          (pixels[offset + 1] - other.pixels[offset + 1]).abs() +
          (pixels[offset + 2] - other.pixels[offset + 2]).abs();
      if (delta > 24) changed += 1;
    }
    return counted == 0 ? 0 : changed / counted;
  }
}
