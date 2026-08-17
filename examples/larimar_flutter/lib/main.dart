import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:larimar/larimar.dart';

void main() => runApp(const LarimarApp());

class LarimarApp extends StatelessWidget {
  const LarimarApp({super.key, this.testStatus, this.testViewSize});

  /// Lets widget tests exercise Flutter composition without creating native
  /// state. The production application always leaves this null.
  final String? testStatus;

  /// Constrains the native view without resizing the host window in integration
  /// tests. Production always leaves this null.
  final Size? testViewSize;

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'Larimar First Light',
    theme: ThemeData(
      colorScheme: ColorScheme.fromSeed(
        seedColor: const Color(0xff46d3b5),
        brightness: Brightness.dark,
      ),
    ),
    home: LarimarHome(testStatus: testStatus, testViewSize: testViewSize),
  );
}

class LarimarHome extends StatefulWidget {
  const LarimarHome({super.key, this.testStatus, this.testViewSize});

  final String? testStatus;
  final Size? testViewSize;

  @override
  State<LarimarHome> createState() => _LarimarHomeState();
}

class _LarimarHomeState extends State<LarimarHome> {
  _PongSession? _pong;
  Object? _error;

  @override
  void initState() {
    super.initState();
    if (widget.testStatus == null) _initialize();
  }

  Future<void> _initialize() async {
    try {
      final session = await _PongSession.create();
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
        : EshiView(
            world: pong.world,
            onFrame: pong.frame,
            onError: (error) {
              if (mounted) setState(() => _error = error);
            },
            placeholder: const ColoredBox(color: Color(0xff05070d)),
          );
    return Scaffold(
      body: Stack(
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
                  const Align(
                    alignment: Alignment.bottomRight,
                    child: Chip(
                      avatar: Icon(Icons.layers_outlined, size: 18),
                      label: Text('Flutter UI over animated Pong'),
                    ),
                  ),
                ],
              ),
            ),
          ),
          if (pong == null && _error == null)
            const Center(child: CircularProgressIndicator()),
        ],
      ),
    );
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

final class _PongSession {
  _PongSession._({
    required this.world,
    required this.material,
    required this.shaderFile,
    required this.leftPaddle,
    required this.rightPaddle,
    required this.ball,
  });

  static const _arenaX = 1.77;
  static const _arenaY = 1.0;
  static const _paddleHalfHeight = 0.2;
  static const _ballRadius = 0.03;

  final LarimarWorld world;
  final LarimarMaterialBuffer material;
  final File shaderFile;
  final int leftPaddle;
  final int rightPaddle;
  final int ball;
  double _hit = 0;
  int _scoreLeft = 0;
  int _scoreRight = 0;

  static Future<_PongSession> create() async {
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
    world.setTransform(
      leftPaddle,
      x: -_arenaX + 0.1,
      y: math.sin(time * 1.2) * 0.55,
    );
    world.setTransform(
      rightPaddle,
      x: _arenaX - 0.1,
      y: math.sin(time * 0.9 + 1.8) * 0.55,
    );
    world.tick(seconds);

    final position = world.transformOf(ball);
    if (position.x > _arenaX + 0.2) {
      _scoreLeft += 1;
      _serve(-1);
    } else if (position.x < -_arenaX - 0.2) {
      _scoreRight += 1;
      _serve(1);
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
    world.dispose();
    if (shaderFile.existsSync()) shaderFile.deleteSync();
  }
}
