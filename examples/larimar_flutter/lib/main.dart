import 'dart:async';
import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:larimar/larimar.dart';

void main() => runApp(const LarimarApp());

class LarimarApp extends StatelessWidget {
  const LarimarApp({
    super.key,
    this.testStatus,
    this.testViewSize,
    this.testWinningScore,
  });

  /// Lets widget tests exercise Flutter composition without creating native
  /// state. The production application always leaves this null.
  final String? testStatus;

  /// Constrains the native view without resizing the host window in integration
  /// tests. Production always leaves this null.
  final Size? testViewSize;

  /// Shortens a match so a test can watch one finish. Production leaves it null
  /// and plays to [_PongSession.winningScore].
  final int? testWinningScore;

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'Larimar First Light',
    theme: ThemeData(
      colorScheme: ColorScheme.fromSeed(
        seedColor: const Color(0xff46d3b5),
        brightness: Brightness.dark,
      ),
    ),
    home: LarimarHome(
      testStatus: testStatus,
      testViewSize: testViewSize,
      testWinningScore: testWinningScore,
    ),
  );
}

class LarimarHome extends StatefulWidget {
  const LarimarHome({
    super.key,
    this.testStatus,
    this.testViewSize,
    this.testWinningScore,
  });

  final String? testStatus;
  final Size? testViewSize;
  final int? testWinningScore;

  @override
  State<LarimarHome> createState() => _LarimarHomeState();
}

class _LarimarHomeState extends State<LarimarHome> {
  _PongSession? _pong;
  Object? _error;
  EshiViewDiagnostics? _diagnostics;
  Timer? _diagnosticsTimer;
  final FocusNode _keyboard = FocusNode(debugLabel: 'larimar-pong');

  @override
  void initState() {
    super.initState();
    if (widget.testStatus == null) {
      _initialize();
      // On screen, "it renders" and "it is composited" are different claims,
      // and only the second one is what a person is looking at. Poll the host
      // so the window says which is true: frames presented, and frames the
      // engine actually borrowed to draw.
      _diagnosticsTimer = Timer.periodic(
        const Duration(milliseconds: 500),
        (_) => _refreshDiagnostics(),
      );
    }
  }

  Future<void> _refreshDiagnostics() async {
    try {
      final diagnostics = await const MacOSEshiViewHost().diagnostics();
      if (mounted) setState(() => _diagnostics = diagnostics);
    } on Object {
      // A host that cannot answer is not worth turning into an error banner
      // over the running game; the counters simply stop updating.
    }
  }

  Future<void> _initialize() async {
    try {
      final session = await _PongSession.create(
        target: widget.testWinningScore ?? _PongSession.winningScore,
      );
      if (!mounted) {
        session.dispose();
        return;
      }
      setState(() => _pong = session);
    } on Object catch (error) {
      if (mounted) setState(() => _error = error);
    }
  }

  @override
  void dispose() {
    _keyboard.dispose();
    _diagnosticsTimer?.cancel();
    _pong?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (widget.testStatus case final status?) {
      return _testShell(context, status);
    }
    final pong = _pong;
    final gameView = pong == null
        ? const ColoredBox(color: Color(0xff05070d))
        // The material's horizontal extent is the view's aspect ratio, so the
        // game has to be told the shape of the window it is being drawn into or
        // its paddles end up outside it.
        : LayoutBuilder(
            builder: (context, constraints) {
              if (constraints.hasBoundedWidth && constraints.maxHeight > 0) {
                pong.aspect = constraints.maxWidth / constraints.maxHeight;
              }
              return EshiView(
                world: pong.world,
                onFrame: pong.frame,
                onError: (error) {
                  if (mounted) setState(() => _error = error);
                },
                placeholder: const ColoredBox(color: Color(0xff05070d)),
              );
            },
          );
    return Scaffold(
      body: Focus(
        focusNode: _keyboard,
        autofocus: true,
        onKeyEvent: _handleKey,
        child: Stack(
          fit: StackFit.expand,
          children: <Widget>[
            if (widget.testViewSize case final size?) ...<Widget>[
              const ColoredBox(color: Color(0xff05070d)),
              Center(
                child: SizedBox(
                  width: size.width,
                  height: size.height,
                  child: gameView,
                ),
              ),
            ] else
              gameView,
            const DecoratedBox(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.topCenter,
                  end: Alignment.center,
                  colors: <Color>[Color(0xaa05070d), Colors.transparent],
                ),
              ),
            ),
            SafeArea(
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    Text(
                      'Larimar First Light',
                      style: Theme.of(context).textTheme.headlineSmall,
                    ),
                    const SizedBox(height: 6),
                    Text(
                      pong == null
                          ? 'Preparing the Metal texture host…'
                          : '${pong.world.backendName} · external texture · '
                                '${pong.world.entityCount} entities',
                    ),
                    if (_diagnostics case final diagnostics?)
                      Text(
                        'presented ${diagnostics.presented} · '
                        'composited ${diagnostics.copies}',
                        key: const ValueKey<String>('larimar-diagnostics'),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    if (_error case final error?) ...<Widget>[
                      const SizedBox(height: 12),
                      DecoratedBox(
                        key: const ValueKey<String>('larimar-error'),
                        decoration: BoxDecoration(
                          color: Theme.of(context).colorScheme.errorContainer,
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: Padding(
                          padding: const EdgeInsets.all(12),
                          child: Text(error.toString()),
                        ),
                      ),
                    ],
                    const Spacer(),
                    Row(
                      children: <Widget>[
                        Text(
                          'W / S move cyan · ↑ / ↓ move magenta · '
                          'first to ${_PongSession.winningScore} wins',
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                        const Spacer(),
                        const Chip(
                          avatar: Icon(Icons.layers_outlined, size: 18),
                          label: Text('Flutter UI over animated Pong'),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            if (pong != null)
              ValueListenableBuilder<String?>(
                valueListenable: pong.winner,
                builder: (context, winner, _) => winner == null
                    ? const SizedBox.shrink()
                    : Center(
                        child: Text(
                          '$winner wins',
                          key: const ValueKey<String>('larimar-winner'),
                          style: Theme.of(context).textTheme.displaySmall,
                        ),
                      ),
              ),
            if (pong == null && _error == null)
              const Center(child: CircularProgressIndicator()),
          ],
        ),
      ),
    );
  }

  /// Held keys set a direction; the game moves the paddle on its own clock.
  ///
  /// The result matters as much as the effect: an unhandled key walks up the
  /// responder chain to AppKit, which beeps at it. Every key this game uses has
  /// to be claimed, or playing it sounds like an error.
  KeyEventResult _handleKey(FocusNode node, KeyEvent event) {
    final pong = _pong;
    if (pong == null) return KeyEventResult.ignored;

    final key = event.logicalKey;
    final ({_Paddle paddle, double direction})? command = switch (key) {
      LogicalKeyboardKey.keyW => (paddle: _Paddle.left, direction: 1),
      LogicalKeyboardKey.keyS => (paddle: _Paddle.left, direction: -1),
      LogicalKeyboardKey.arrowUp => (paddle: _Paddle.right, direction: 1),
      LogicalKeyboardKey.arrowDown => (paddle: _Paddle.right, direction: -1),
      _ => null,
    };
    if (command == null) return KeyEventResult.ignored;

    pong.steer(command.paddle, event is KeyUpEvent ? 0 : command.direction);
    return KeyEventResult.handled;
  }

  Widget _testShell(BuildContext context, String status) => Scaffold(
    appBar: AppBar(title: const Text('Larimar First Light')),
    body: Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: <Widget>[
          const Text('Native core online'),
          Text(status),
          FilledButton(
            onPressed: null,
            child: const Text('Advance one simulation tick'),
          ),
        ],
      ),
    ),
  );
}

enum _Paddle { left, right }

/// One player's paddle: an idle sweep until a key claims it, then a position.
final class _Side {
  double input = 0;
  bool manual = false;
  double _y = 0;

  double advance(double seconds, double limit, double idle) {
    _y = manual
        ? (_y + input * _PongSession._paddleSpeed * seconds).clamp(
            -limit,
            limit,
          )
        : idle;
    return _y;
  }
}

final class _PongSession {
  _PongSession._({
    required this.world,
    required this.material,
    required this.shaderFile,
    required this.leftPaddle,
    required this.rightPaddle,
    required this.ball,
    required int target,
  }) : _target = target;

  /// The arena's half-width at 16:9, and the width the walls were built for.
  static const _arenaX = 1.77;
  static const _arenaY = 1.0;
  static const _paddleHalfHeight = 0.2;
  static const _ballRadius = 0.03;
  static const _paddleSpeed = 2.2;

  final LarimarWorld world;
  final LarimarMaterialBuffer material;
  final File shaderFile;
  final int leftPaddle;
  final int rightPaddle;
  final int ball;

  /// The score that ends this match; production plays to [winningScore].
  final int _target;
  double _hit = 0;
  int _scoreLeft = 0;
  int _scoreRight = 0;

  /// Half-width of the visible arena, in the shader's coordinates.
  ///
  /// The material works in `uv = (fragCoord * 2 - iResolution) / iResolution.y`,
  /// so the horizontal extent is the view's aspect ratio and *only* the vertical
  /// one is fixed. Pinning the paddles to a 16:9 constant put them off screen in
  /// any window narrower than that — which is every default window, since
  /// Flutter opens at 4:3. The view tells the session its aspect instead.
  double _halfWidth = _arenaX;

  /// The score that ends a match, and the number of dots the material draws.
  static const winningScore = 9;

  final _Side _left = _Side();
  final _Side _right = _Side();

  /// The winner of the last completed match, while it is being announced.
  final ValueNotifier<String?> winner = ValueNotifier<String?>(null);
  double _celebration = 0;

  set aspect(double value) {
    if (value.isFinite && value > 0.5) _halfWidth = value;
  }

  /// A held key becomes a direction; the first one hands that paddle over to
  /// the player for good, so it does not snap back to its idle sweep.
  void steer(_Paddle paddle, double direction) {
    final side = paddle == _Paddle.left ? _left : _right;
    side.input = direction;
    if (direction != 0) side.manual = true;
  }

  static Future<_PongSession> create({int target = winningScore}) async {
    final source = await rootBundle.loadString(
      'packages/larimar/assets/pong.gpu.cpp',
    );
    final file = File(
      '${Directory.systemTemp.path}/larimar-pong-$pid-${DateTime.now().microsecondsSinceEpoch}.gpu.cpp',
    );
    await file.writeAsString(source, flush: true);

    LarimarWorld? world;
    try {
      world = LarimarWorld(
        config: const LarimarWorldConfig(
          width: 960,
          height: 540,
          grade: LarimarGrade.brush,
        ),
      );
      world.scene(minimumWordCapacity: 128)
        ..node(1)
        ..transform(x: -_arenaX + 0.1, y: 0)
        ..collider(
          halfWidth: 0.02,
          halfHeight: _paddleHalfHeight,
          layer: 2,
          mask: 1,
          flags: LarimarColliderFlags.staticBody,
        )
        ..node(2)
        ..transform(x: _arenaX - 0.1, y: 0)
        ..collider(
          halfWidth: 0.02,
          halfHeight: _paddleHalfHeight,
          layer: 2,
          mask: 1,
          flags: LarimarColliderFlags.staticBody,
        )
        ..node(3)
        ..transform(x: 0, y: 0)
        ..velocity(x: 1.1, y: 0.27)
        ..collider(
          halfWidth: _ballRadius,
          halfHeight: _ballRadius,
          layer: 1,
          mask: 2 | 4,
        )
        ..restitution(1)
        ..node(4)
        ..transform(x: 0, y: _arenaY + 0.1)
        ..collider(
          halfWidth: _arenaX + 1,
          halfHeight: 0.1,
          layer: 4,
          mask: 1,
          flags: LarimarColliderFlags.staticBody,
        )
        ..node(5)
        ..transform(x: 0, y: -_arenaY - 0.1)
        ..collider(
          halfWidth: _arenaX + 1,
          halfHeight: 0.1,
          layer: 4,
          mask: 1,
          flags: LarimarColliderFlags.staticBody,
        )
        ..finish();

      final material = world.bindShaderMaterial(
        sourcePath: file.path,
        uniformFloatCount: 9,
      );
      final session = _PongSession._(
        target: target,
        world: world,
        material: material,
        shaderFile: file,
        leftPaddle: world.sceneEntity(1),
        rightPaddle: world.sceneEntity(2),
        ball: world.sceneEntity(3),
      );
      session._updateUniforms();
      return session;
    } on Object {
      world?.dispose();
      if (await file.exists()) await file.delete();
      rethrow;
    }
  }

  void frame(LarimarWorld world, Duration elapsed, Duration delta) {
    final seconds = (delta.inMicroseconds / Duration.microsecondsPerSecond)
        .clamp(0.0, 0.1);
    final time = elapsed.inMicroseconds / Duration.microsecondsPerSecond;

    // Each paddle plays itself until somebody takes it over, and then stays
    // where they left it.
    final limit = _arenaY - _paddleHalfHeight;
    world.setTransform(
      leftPaddle,
      x: -_halfWidth + 0.1,
      y: _left.advance(seconds, limit, math.sin(time * 1.2) * 0.55),
    );
    world.setTransform(
      rightPaddle,
      x: _halfWidth - 0.1,
      y: _right.advance(seconds, limit, math.sin(time * 0.9 + 1.8) * 0.55),
    );
    world.tick(seconds);

    if (_celebration > 0) {
      // The match is over; hold the ball at centre until the announcement ends.
      _celebration -= seconds;
      if (_celebration <= 0) {
        _scoreLeft = 0;
        _scoreRight = 0;
        winner.value = null;
        _serve(1);
      }
    } else {
      final position = world.transformOf(ball);
      if (position.x > _halfWidth + 0.2) {
        _scoreLeft += 1;
        _serve(-1);
      } else if (position.x < -_halfWidth - 0.2) {
        _scoreRight += 1;
        _serve(1);
      }
      // A game with no end is a demo that scores past the nine dots the
      // material can draw and then keeps going, which is what this did.
      if (_scoreLeft >= _target || _scoreRight >= _target) {
        winner.value = _scoreLeft > _scoreRight ? 'Cyan' : 'Magenta';
        _celebration = 2.5;
        world.setTransform(ball, x: 0, y: 0);
        world.setVelocity(ball, x: 0, y: 0);
      }
    }
    final collisions = world.events().drain().events;
    if (collisions.any(
      (event) =>
          event is LarimarCollisionEvent &&
          (event.a == leftPaddle ||
              event.b == leftPaddle ||
              event.a == rightPaddle ||
              event.b == rightPaddle),
    )) {
      _hit = 1;
    }
    _hit *= math.pow(0.05, seconds).toDouble();
    _updateUniforms();
  }

  void _serve(double direction) {
    world.setTransform(ball, x: 0, y: 0);
    world.setVelocity(ball, x: direction * 1.1, y: 0.31);
  }

  void _updateUniforms() {
    material.requireActive();
    final left = world.transformOf(leftPaddle);
    final right = world.transformOf(rightPaddle);
    final ballPosition = world.transformOf(ball);
    material.values
      ..[0] = left.x
      ..[1] = left.y
      ..[2] = right.x
      ..[3] = right.y
      ..[4] = ballPosition.x
      ..[5] = ballPosition.y
      ..[6] = _hit
      ..[7] = _scoreLeft.toDouble()
      ..[8] = _scoreRight.toDouble();
  }

  void dispose() {
    winner.dispose();
    world.dispose();
    if (shaderFile.existsSync()) shaderFile.deleteSync();
  }
}
