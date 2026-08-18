import 'dart:async';
import 'dart:io';
import 'dart:math' as math;

import 'package:audioplayers/audioplayers.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:larimar/larimar.dart';

const _modelAsset = 'assets/larimar-model.glb';
const _audioAsset = 'assets/larimar-official.mp3';
const _soundDuration = Duration(microseconds: 4544313);

void main() => runApp(const LarimarBrandApp());

class LarimarBrandApp extends StatelessWidget {
  const LarimarBrandApp({super.key, this.testMode = false});

  final bool testMode;

  @override
  Widget build(BuildContext context) => MaterialApp(
    debugShowCheckedModeBanner: false,
    title: 'larimar',
    theme: ThemeData(
      brightness: Brightness.dark,
      fontFamily: 'HarmonyOS Sans',
      scaffoldBackgroundColor: const Color(0xff03070b),
      colorScheme: const ColorScheme.dark(
        primary: Color(0xff71e6e4),
        surface: Color(0xff07131a),
      ),
    ),
    home: _LarimarBrandHome(testMode: testMode),
  );
}

class _LarimarBrandHome extends StatefulWidget {
  const _LarimarBrandHome({required this.testMode});

  final bool testMode;

  @override
  State<_LarimarBrandHome> createState() => _LarimarBrandHomeState();
}

class _LarimarBrandHomeState extends State<_LarimarBrandHome>
    with SingleTickerProviderStateMixin {
  late final AnimationController _sequence;
  final AudioPlayer _audio = AudioPlayer(playerId: 'larimar-brand');
  _LarimarHeroSession? _session;
  Directory? _soundtrackDirectory;
  Object? _error;
  bool _muted = false;

  @override
  void initState() {
    super.initState();
    _sequence = AnimationController(vsync: this, duration: _soundDuration);
    if (widget.testMode) {
      _sequence.value = 0.48;
    } else {
      unawaited(_initialize());
    }
  }

  Future<void> _initialize() async {
    try {
      final session = await _LarimarHeroSession.create();
      if (!mounted) {
        session.dispose();
        return;
      }
      setState(() => _session = session);
      await WidgetsBinding.instance.endOfFrame;
      if (!mounted) return;

      final soundtrack = await _stageSoundtrack();
      await _audio.setReleaseMode(ReleaseMode.loop);
      await _audio.setVolume(1);
      await _audio.play(DeviceFileSource(soundtrack.path));
      _sequence.repeat(period: _soundDuration);
    } on Object catch (error) {
      if (mounted) setState(() => _error = error);
    }
  }

  Future<File> _stageSoundtrack() async {
    final data = await rootBundle.load(_audioAsset);
    final directory = await Directory.systemTemp.createTemp('larimar-audio-');
    final file = File('${directory.path}/larimar-official.mp3');
    await file.writeAsBytes(
      data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes),
      flush: true,
    );
    _soundtrackDirectory = directory;
    return file;
  }

  Future<void> _toggleMute() async {
    final muted = !_muted;
    setState(() => _muted = muted);
    await _audio.setVolume(muted ? 0 : 1);
  }

  @override
  void dispose() {
    _sequence.dispose();
    unawaited(_audio.dispose());
    final soundtrackDirectory = _soundtrackDirectory;
    if (soundtrackDirectory != null && soundtrackDirectory.existsSync()) {
      soundtrackDirectory.deleteSync(recursive: true);
    }
    _session?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    body: AnimatedBuilder(
      animation: _sequence,
      builder: (context, _) => _LarimarBackdrop(
        phase: _sequence.value,
        child: SafeArea(
          child: Stack(
            fit: StackFit.expand,
            children: <Widget>[
              Padding(
                padding: const EdgeInsets.symmetric(
                  horizontal: 54,
                  vertical: 40,
                ),
                child: LayoutBuilder(
                  builder: (context, constraints) =>
                      _brandLockup(vertical: constraints.maxWidth < 600),
                ),
              ),
              Positioned(
                right: 24,
                bottom: 22,
                child: _SoundButton(muted: _muted, onPressed: _toggleMute),
              ),
              if (_error case final error?)
                Positioned(
                  left: 24,
                  right: 24,
                  bottom: 24,
                  child: _ErrorBanner(error: error),
                ),
            ],
          ),
        ),
      ),
    ),
  );

  Widget _brandLockup({required bool vertical}) {
    final session = _session;
    final model = Semantics(
      label: 'Larimar logo rotating gently',
      image: true,
      child: SizedBox.square(
        dimension: vertical ? 300 : 360,
        child: session == null
            ? Center(
                child: SizedBox.square(
                  dimension: 22,
                  child: CircularProgressIndicator(
                    strokeWidth: 1.5,
                    color: Colors.white.withValues(alpha: 0.52),
                  ),
                ),
              )
            : EshiView(
                world: session.world,
                onFrame: session.frame,
                onError: (error) {
                  if (mounted) setState(() => _error = error);
                },
                placeholder: const SizedBox.expand(),
              ),
      ),
    );
    const wordmark = _Wordmark();

    if (vertical) {
      return Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: <Widget>[
          model,
          Transform.translate(offset: const Offset(0, -26), child: wordmark),
        ],
      );
    }
    return Row(
      mainAxisAlignment: MainAxisAlignment.center,
      children: <Widget>[
        Flexible(child: model),
        const SizedBox(width: 8),
        wordmark,
      ],
    );
  }
}

final class _LarimarHeroSession {
  _LarimarHeroSession._({required this.world, required this.asset});

  final LarimarWorld world;
  final LarimarAsset asset;

  static Future<_LarimarHeroSession> create() async {
    final world = LarimarWorld(
      config: const LarimarWorldConfig(
        width: 720,
        height: 720,
        grade: LarimarGrade.brush,
      ),
    );
    try {
      final asset = await world.loadAssetBundle(_modelAsset);
      asset.instantiate(scale: 0.86, spinY: 0.32);
      return _LarimarHeroSession._(world: world, asset: asset);
    } on Object {
      world.dispose();
      rethrow;
    }
  }

  void frame(LarimarWorld world, Duration elapsed, Duration delta) {
    final seconds = delta.inMicroseconds / Duration.microsecondsPerSecond;
    world.tick(seconds.clamp(0, 0.1));
  }

  void dispose() {
    asset.dispose();
    world.dispose();
  }
}

class _Wordmark extends StatelessWidget {
  const _Wordmark();

  @override
  Widget build(BuildContext context) => Semantics(
    header: true,
    child: Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: <Widget>[
        Text(
          'larimar',
          key: const ValueKey<String>('larimar-wordmark'),
          style: const TextStyle(
            color: Color(0xfff5fbfb),
            fontSize: 92,
            height: 0.94,
            fontWeight: FontWeight.w300,
            letterSpacing: -4.8,
          ),
        ),
        const SizedBox(height: 18),
        Text(
          'A GAME ENGINE IN MOTION',
          style: TextStyle(
            color: Colors.white.withValues(alpha: 0.54),
            fontSize: 11,
            height: 1,
            fontWeight: FontWeight.w500,
            letterSpacing: 3.35,
          ),
        ),
      ],
    ),
  );
}

class _LarimarBackdrop extends StatelessWidget {
  const _LarimarBackdrop({required this.phase, required this.child});

  final double phase;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final story = _GradientStory.at(phase);
    final drift = math.sin(phase * math.pi * 2);
    return DecoratedBox(
      decoration: BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment(-0.95 + drift * 0.08, -0.9),
          end: Alignment(0.82, 0.9 - drift * 0.06),
          colors: story.base,
          stops: const <double>[0, 0.37, 0.72, 1],
        ),
      ),
      child: Stack(
        fit: StackFit.expand,
        children: <Widget>[
          DecoratedBox(
            decoration: BoxDecoration(
              gradient: RadialGradient(
                center: Alignment(-0.82 + phase * 1.58, -0.46 + drift * 0.16),
                radius: 0.62 + story.energy * 0.28,
                colors: <Color>[
                  story.glow.withValues(alpha: 0.28 * story.energy),
                  story.glow.withValues(alpha: 0),
                ],
              ),
            ),
          ),
          DecoratedBox(
            decoration: BoxDecoration(
              gradient: RadialGradient(
                center: Alignment(0.72 - phase * 0.62, 0.78),
                radius: 0.82,
                colors: <Color>[
                  story.echo.withValues(alpha: 0.2 * story.energy),
                  story.echo.withValues(alpha: 0),
                ],
              ),
            ),
          ),
          child,
        ],
      ),
    );
  }
}

class _GradientStory {
  const _GradientStory({
    required this.base,
    required this.glow,
    required this.echo,
    required this.energy,
  });

  final List<Color> base;
  final Color glow;
  final Color echo;
  final double energy;

  static const _quiet = <Color>[
    Color(0xff02060a),
    Color(0xff07141b),
    Color(0xff071019),
    Color(0xff020508),
  ];
  static const _rise = <Color>[
    Color(0xff031018),
    Color(0xff063641),
    Color(0xff12303f),
    Color(0xff050712),
  ];
  static const _shimmer = <Color>[
    Color(0xff04151d),
    Color(0xff0b5360),
    Color(0xff292453),
    Color(0xff090713),
  ];

  factory _GradientStory.at(double phase) {
    if (phase < 0.47) {
      final t = Curves.easeInOutCubic.transform(phase / 0.47);
      return _GradientStory(
        base: _lerpColors(_quiet, _rise, t),
        glow: Color.lerp(const Color(0xff1e6d78), const Color(0xff86f7ee), t)!,
        echo: const Color(0xff5c67c8),
        energy: 0.25 + t * 0.75,
      );
    }
    if (phase < 0.74) {
      final t = Curves.easeOutCubic.transform((phase - 0.47) / 0.27);
      return _GradientStory(
        base: _lerpColors(_rise, _shimmer, t),
        glow: Color.lerp(const Color(0xff86f7ee), const Color(0xffacdfff), t)!,
        echo: Color.lerp(const Color(0xff5967d7), const Color(0xff9d72de), t)!,
        energy: 1,
      );
    }
    final t = Curves.easeInCubic.transform((phase - 0.74) / 0.26);
    return _GradientStory(
      base: _lerpColors(_shimmer, _quiet, t),
      glow: Color.lerp(const Color(0xffacdfff), const Color(0xff1e6d78), t)!,
      echo: Color.lerp(const Color(0xff9d72de), const Color(0xff293057), t)!,
      energy: 1 - t * 0.75,
    );
  }

  static List<Color> _lerpColors(List<Color> a, List<Color> b, double t) =>
      List<Color>.generate(
        a.length,
        (index) => Color.lerp(a[index], b[index], t)!,
      );
}

class _SoundButton extends StatelessWidget {
  const _SoundButton({required this.muted, required this.onPressed});

  final bool muted;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) => Semantics(
    button: true,
    label: muted ? 'Unmute soundtrack' : 'Mute soundtrack',
    child: IconButton(
      onPressed: onPressed,
      tooltip: muted ? 'Unmute' : 'Mute',
      color: Colors.white.withValues(alpha: 0.62),
      icon: Icon(muted ? Icons.volume_off_outlined : Icons.volume_up_outlined),
    ),
  );
}

class _ErrorBanner extends StatelessWidget {
  const _ErrorBanner({required this.error});

  final Object error;

  @override
  Widget build(BuildContext context) => Align(
    alignment: Alignment.bottomCenter,
    child: DecoratedBox(
      decoration: BoxDecoration(
        color: const Color(0xe61b1016),
        border: Border.all(color: const Color(0x55ff8da1)),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        child: Text(
          error.toString(),
          key: const ValueKey<String>('larimar-hero-error'),
          style: const TextStyle(fontSize: 12, color: Color(0xffffc6cf)),
        ),
      ),
    ),
  );
}
