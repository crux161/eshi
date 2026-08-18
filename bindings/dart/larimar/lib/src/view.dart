part of '../larimar.dart';

const _textureChannel = MethodChannel('dev.larimar/texture');
const _nativeAssetId =
    'package:larimar/src/generated/larimar_bindings_generated.dart';

@ffi.Native<
  ffi.Int Function(
    ffi.Pointer<native.EshiWorld>,
    ffi.Pointer<ffi.Void>,
    ffi.Float,
  )
>(assetId: _nativeAssetId, symbol: 'eshi__metal_render_texture')
external int _eshiMetalRenderTexture(
  ffi.Pointer<native.EshiWorld> world,
  ffi.Pointer<ffi.Void> metalTexture,
  double time,
);

/// The per-view host resources returned by the macOS texture adapter.
@immutable
final class EshiViewSurface {
  const EshiViewSurface({
    required this.textureId,
    required this.metalTexture,
    required this.width,
    required this.height,
  });

  final int textureId;
  final int metalTexture;
  final int width;
  final int height;
}

/// The macOS texture host's surface accounting, for lifecycle leak gates.
///
/// [registered] counts the surfaces the host still owns; [live] counts the ones
/// that have not been deallocated yet, which stays above zero for as long as
/// Flutter's raster thread is still borrowing a disposed surface. A lifecycle
/// loop has reclaimed everything only when both reach zero and
/// [liveSurfaceBytes] follows them down.
///
/// [copies] is the one that answers "is anything on screen": the engine borrows
/// the surface only while it is compositing the texture layer, so [presented]
/// climbing while [copies] stays at zero means frames are being produced into a
/// view nobody is drawing.
@immutable
final class EshiViewDiagnostics {
  const EshiViewDiagnostics({
    required this.created,
    required this.resized,
    required this.presented,
    required this.disposed,
    required this.suspensions,
    required this.registered,
    required this.live,
    required this.liveSurfaceBytes,
    required this.copies,
    required this.metalAvailable,
  });

  final int created;
  final int resized;
  final int presented;
  final int disposed;
  final int suspensions;
  final int registered;
  final int live;
  final int liveSurfaceBytes;
  final int copies;
  final bool metalAvailable;

  @override
  String toString() =>
      'EshiViewDiagnostics(created: $created, resized: $resized, '
      'presented: $presented, disposed: $disposed, '
      'suspensions: $suspensions, registered: $registered, live: $live, '
      'liveSurfaceBytes: $liveSurfaceBytes, copies: $copies, '
      'metalAvailable: $metalAvailable)';
}

/// Host seam kept public for deterministic widget and embedder tests.
abstract interface class EshiViewHost {
  Future<EshiViewSurface> create({required int width, required int height});
  Future<EshiViewSurface> resize(
    EshiViewSurface surface, {
    required int width,
    required int height,
  });
  void render(EshiViewSurface surface, LarimarWorld world, double time);
  Future<void> present(EshiViewSurface surface);
  Future<void> setSuspended(EshiViewSurface surface, bool suspended);
  Future<void> dispose(EshiViewSurface surface);
}

/// Flutter method-channel host backed by an IOSurface and Metal texture.
final class MacOSEshiViewHost implements EshiViewHost {
  const MacOSEshiViewHost();

  @override
  Future<EshiViewSurface> create({
    required int width,
    required int height,
  }) async {
    final value = await _textureChannel.invokeMapMethod<String, Object?>(
      'create',
      <String, Object?>{'width': width, 'height': height},
    );
    return _surfaceFromMap(value);
  }

  @override
  Future<EshiViewSurface> resize(
    EshiViewSurface surface, {
    required int width,
    required int height,
  }) async {
    final value = await _textureChannel.invokeMapMethod<String, Object?>(
      'resize',
      <String, Object?>{
        'textureId': surface.textureId,
        'width': width,
        'height': height,
      },
    );
    return _surfaceFromMap(value);
  }

  @override
  void render(EshiViewSurface surface, LarimarWorld world, double time) {
    final result = _eshiMetalRenderTexture(
      world._requirePointer(),
      ffi.Pointer<ffi.Void>.fromAddress(surface.metalTexture),
      time,
    );
    _checkNative(
      native.EshiResult.fromValue(result),
      'eshi__metal_render_texture',
    );
  }

  @override
  Future<void> present(EshiViewSurface surface) => _textureChannel.invokeMethod(
    'present',
    <String, Object?>{'textureId': surface.textureId},
  );

  @override
  Future<void> setSuspended(EshiViewSurface surface, bool suspended) =>
      _textureChannel.invokeMethod('setSuspended', <String, Object?>{
        'textureId': surface.textureId,
        'suspended': suspended,
      });

  @override
  Future<void> dispose(EshiViewSurface surface) => _textureChannel.invokeMethod(
    'dispose',
    <String, Object?>{'textureId': surface.textureId},
  );

  /// Reads the host's surface accounting. [EshiView] never calls this; it
  /// exists so a lifecycle test can prove the host let go of what it allocated.
  Future<EshiViewDiagnostics> diagnostics() async {
    final value = await _textureChannel.invokeMapMethod<String, Object?>(
      'diagnostics',
    );
    if (value == null) {
      throw PlatformException(
        code: 'invalid-diagnostics',
        message: 'The macOS texture host returned no diagnostics.',
      );
    }
    int count(String name) => value[name] is int ? value[name]! as int : 0;
    return EshiViewDiagnostics(
      created: count('created'),
      resized: count('resized'),
      presented: count('presented'),
      disposed: count('disposed'),
      suspensions: count('suspensions'),
      registered: count('registered'),
      live: count('live'),
      liveSurfaceBytes: count('liveSurfaceBytes'),
      copies: count('copies'),
      metalAvailable: value['metalAvailable'] == true,
    );
  }

  /// Writes the view's surface to [path] as a PNG, bypassing the compositor.
  ///
  /// The pixels Larimar submitted, as the host holds them — which is the only
  /// way to tell an empty surface apart from a layer the engine did not draw.
  Future<void> captureSurface(EshiViewSurface surface, String path) =>
      _textureChannel.invokeMethod('captureSurface', <String, Object?>{
        'textureId': surface.textureId,
        'path': path,
      });

  EshiViewSurface _surfaceFromMap(Map<String, Object?>? value) {
    if (value == null) {
      throw PlatformException(
        code: 'invalid-surface',
        message: 'The macOS texture host returned no surface.',
      );
    }
    int field(String name) {
      final fieldValue = value[name];
      if (fieldValue is! int) {
        throw PlatformException(
          code: 'invalid-surface',
          message: 'The macOS texture host returned an invalid $name.',
        );
      }
      return fieldValue;
    }

    return EshiViewSurface(
      textureId: field('textureId'),
      metalTexture: field('metalTexture'),
      width: field('width'),
      height: field('height'),
    );
  }
}

typedef EshiViewFrameCallback =
    void Function(LarimarWorld world, Duration elapsed, Duration delta);

/// Composes one Larimar camera/swapchain surface into an ordinary Flutter tree.
///
/// Native world access and Metal submission occur synchronously on the world's
/// owner isolate. Registration, resize, frame notification, suspension, and
/// disposal are serialized through the platform channel. Flutter's raster
/// thread only receives an owning CVPixelBuffer reference.
final class EshiView extends StatefulWidget {
  const EshiView({
    super.key,
    required this.world,
    this.onFrame,
    this.onError,
    this.placeholder = const SizedBox.expand(),
    this.host = const MacOSEshiViewHost(),
  });

  final LarimarWorld world;
  final EshiViewFrameCallback? onFrame;
  final ValueChanged<Object>? onError;
  final Widget placeholder;

  @visibleForTesting
  final EshiViewHost host;

  @override
  State<EshiView> createState() => _EshiViewState();
}

final class _EshiViewState extends State<EshiView>
    with SingleTickerProviderStateMixin, WidgetsBindingObserver {
  late final Ticker _ticker;
  EshiViewSurface? _surface;
  Size? _requestedPhysicalSize;
  Duration? _lastElapsed;
  Future<void> _operations = Future<void>.value();
  bool _frameInFlight = false;
  bool _suspended = false;
  bool _disposed = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _ticker = createTicker(_frame);
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    final suspended = state != AppLifecycleState.resumed;
    if (_suspended == suspended) return;
    _suspended = suspended;
    _lastElapsed = null;
    final surface = _surface;
    if (surface != null) {
      _serialize(() => widget.host.setSuspended(surface, suspended));
    }
  }

  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, constraints) {
      final logicalWidth = constraints.maxWidth.isFinite
          ? constraints.maxWidth
          : constraints.minWidth;
      final logicalHeight = constraints.maxHeight.isFinite
          ? constraints.maxHeight
          : constraints.minHeight;
      if (logicalWidth > 0 && logicalHeight > 0) {
        final ratio = MediaQuery.devicePixelRatioOf(context);
        final physical = Size(
          (logicalWidth * ratio).ceilToDouble(),
          (logicalHeight * ratio).ceilToDouble(),
        );
        if (_requestedPhysicalSize != physical) {
          _requestedPhysicalSize = physical;
          WidgetsBinding.instance.addPostFrameCallback((_) {
            if (!_disposed) _ensureSurface(physical);
          });
        }
      }
      final surface = _surface;
      if (surface == null) return widget.placeholder;
      return Texture(textureId: surface.textureId, freeze: _suspended);
    },
  );

  void _ensureSurface(Size physicalSize) {
    final width = physicalSize.width.toInt().clamp(1, 16384);
    final height = physicalSize.height.toInt().clamp(1, 16384);
    _serialize(() async {
      var surface = _surface;
      if (surface == null) {
        surface = await widget.host.create(width: width, height: height);
        if (_disposed) {
          await widget.host.dispose(surface);
          return;
        }
        if (_suspended) await widget.host.setSuspended(surface, true);
      } else if (surface.width != width || surface.height != height) {
        surface = await widget.host.resize(
          surface,
          width: width,
          height: height,
        );
      }
      if (_disposed) return;
      if (!identical(_surface, surface)) {
        setState(() => _surface = surface);
      }
      if (!_ticker.isActive) _ticker.start();
    });
  }

  void _frame(Duration elapsed) {
    final surface = _surface;
    if (surface == null || _frameInFlight || _suspended || _disposed) return;
    final previous = _lastElapsed;
    final delta = previous == null ? Duration.zero : elapsed - previous;
    _lastElapsed = elapsed;
    try {
      widget.onFrame?.call(widget.world, elapsed, delta);
      widget.host.render(
        surface,
        widget.world,
        elapsed.inMicroseconds / Duration.microsecondsPerSecond,
      );
    } on Object catch (error) {
      _report(error);
      return;
    }
    _frameInFlight = true;
    widget.host.present(surface).catchError(_report).whenComplete(() {
      _frameInFlight = false;
    });
  }

  void _serialize(Future<void> Function() operation) {
    _operations = _operations.then((_) => operation()).catchError(_report);
  }

  void _report(Object error) {
    if (!_disposed && _ticker.isActive) _ticker.stop();
    widget.onError?.call(error);
    if (widget.onError == null) {
      FlutterError.reportError(
        FlutterErrorDetails(exception: error, library: 'larimar EshiView'),
      );
    }
  }

  @override
  void dispose() {
    _disposed = true;
    WidgetsBinding.instance.removeObserver(this);
    _ticker.dispose();
    final surface = _surface;
    _surface = null;
    if (surface != null) {
      _serialize(() => widget.host.dispose(surface));
    }
    super.dispose();
  }
}
