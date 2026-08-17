import 'package:flutter/material.dart';
import 'package:larimar/larimar.dart';

void main() => runApp(const LarimarApp());

class LarimarApp extends StatelessWidget {
  const LarimarApp({super.key, this.testStatus});

  /// Lets widget tests exercise Flutter composition without creating native
  /// state. The production application always leaves this null.
  final String? testStatus;

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'Larimar First Light',
    theme: ThemeData(
      colorScheme: ColorScheme.fromSeed(
        seedColor: const Color(0xff46d3b5),
        brightness: Brightness.dark,
      ),
    ),
    home: LarimarHome(testStatus: testStatus),
  );
}

class LarimarHome extends StatefulWidget {
  const LarimarHome({super.key, this.testStatus});

  final String? testStatus;

  @override
  State<LarimarHome> createState() => _LarimarHomeState();
}

class _LarimarHomeState extends State<LarimarHome> {
  LarimarWorld? _world;
  late String _status;

  @override
  void initState() {
    super.initState();
    if (widget.testStatus case final status?) {
      _status = status;
      return;
    }
    final world = _world = LarimarWorld();
    world.scene()
      ..node(1)
      ..transform(x: 0, y: 0)
      ..finish();
    _status = _describe(world);
  }

  @override
  void dispose() {
    _world?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('Larimar First Light')),
    body: Center(
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 560),
        child: Padding(
          padding: const EdgeInsets.all(32),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: <Widget>[
              const Icon(Icons.blur_on, size: 72),
              const SizedBox(height: 24),
              Text(
                'Native core online',
                style: Theme.of(context).textTheme.headlineMedium,
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 12),
              Text(_status, textAlign: TextAlign.center),
              const SizedBox(height: 24),
              const Text(
                'The EshiView external texture lands in Plan Step 4. This '
                'reference shell already proves ABI loading, retained-scene '
                'flushes, lifecycle ownership, and Flutter composition.',
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 24),
              FilledButton.icon(
                onPressed: _world == null
                    ? null
                    : () {
                        setState(() {
                          _world!.tick();
                          _status = _describe(_world!);
                        });
                      },
                icon: const Icon(Icons.skip_next),
                label: const Text('Advance one simulation tick'),
              ),
            ],
          ),
        ),
      ),
    ),
  );

  String _describe(LarimarWorld world) =>
      '${world.backendName} · ${world.entityCount} entity · '
      'frame ${world.frameIndex}';
}
