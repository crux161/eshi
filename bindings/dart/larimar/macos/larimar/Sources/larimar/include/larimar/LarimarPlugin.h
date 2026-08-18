#import <FlutterMacOS/FlutterMacOS.h>

@interface LarimarPlugin : NSObject <FlutterPlugin, FlutterAppLifecycleDelegate>

/// Host seam kept public so diagnostics harnesses can drive the texture adapter
/// with a stub registry, without an engine, a window, or a Dart isolate.
- (instancetype)initWithRegistry:(id<FlutterTextureRegistry>)registry;

@end
