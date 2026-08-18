/**
 * @file texture_host_main.mm
 * @brief Leak and race harness for the macOS EshiView texture adapter.
 *
 * The widget lifecycle test in examples/larimar_flutter proves the adapter
 * survives real create/resize/suspend/destroy cycles, but it runs inside a
 * Flutter engine whose raster thread is timing-dependent: a lock that is merely
 * *usually* held would pass it. And it runs under the App Sandbox, where a
 * process cannot open its own task port, so `leaks` cannot examine it from
 * within. This harness removes the engine instead of the concurrency. It drives
 * the real LarimarPlugin through its real method channel entry points from the
 * platform thread while a stand-in raster queue borrows surfaces through
 * `copyPixelBuffer`, exactly as Flutter's compositor does.
 *
 * scripts/check_eshiview_host.sh builds it twice: once plain, run under
 * `leaks --atExit`, and once with -fsanitize=thread.
 *
 * The stub registry mirrors Flutter's threading contract rather than
 * simplifying it: registration and unregistration are requested on the platform
 * thread and applied on the raster queue, `textureFrameAvailable:` copies the
 * surface there, and `onTextureUnregistered:` is delivered there too. Its own
 * state is confined to that queue so that any race the sanitizer reports
 * belongs to the adapter under test and not to the harness.
 *
 * Exit codes: 0 clean (or no Metal device, reported as a skip), 66 on a
 * sanitizer report, 1 on a lifecycle invariant failure. `leaks --atExit`
 * discards the child's code, so the verdict is also printed as
 * `[host] status=...`.
 */
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <larimar/LarimarPlugin.h>

namespace {

const int kDefaultCycles = 24;
const NSInteger kWidth = 640;
const NSInteger kHeight = 360;

int IntegerFromEnvironment(const char* name, int fallback) {
  const char* value = getenv(name);
  if (!value || !*value) return fallback;
  int parsed = atoi(value);
  return parsed > 0 ? parsed : fallback;
}

}  // namespace

/// Stands in for `FlutterTextureRegistry`, preserving its thread contract.
@interface RasterRegistry : NSObject <FlutterTextureRegistry>
@property(nonatomic, readonly) dispatch_queue_t rasterQueue;
- (void)drain;
- (uint64_t)framesCopied;
@end

@implementation RasterRegistry {
  dispatch_queue_t _raster;
  /// Raster-queue confined, like the engine's own texture map.
  NSMutableDictionary<NSNumber*, NSObject<FlutterTexture>*>* _textures;
  uint64_t _framesCopied;
  /// Platform-thread confined.
  int64_t _nextTextureId;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _raster = dispatch_queue_create("dev.larimar.harness.raster",
                                    DISPATCH_QUEUE_SERIAL);
    _textures = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (dispatch_queue_t)rasterQueue {
  return _raster;
}

- (int64_t)registerTexture:(NSObject<FlutterTexture>*)texture {
  int64_t textureId = ++_nextTextureId;
  dispatch_async(_raster, ^{
    self->_textures[@(textureId)] = texture;
  });
  return textureId;
}

- (void)textureFrameAvailable:(int64_t)textureId {
  dispatch_async(_raster, ^{
    @autoreleasepool {
      NSObject<FlutterTexture>* texture = self->_textures[@(textureId)];
      if (!texture) return;
      CVPixelBufferRef buffer = [texture copyPixelBuffer];
      if (!buffer) return;
      // Read the surface the way a compositor would before handing it back.
      // The copy above must have returned an owning reference, or this touches
      // memory the platform thread is free to have released underneath us.
      if (CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly) ==
          kCVReturnSuccess) {
        const uint8_t* pixels =
            static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(buffer));
        if (pixels) {
          volatile uint8_t sample = pixels[0];
          (void)sample;
          self->_framesCopied += 1;
        }
        CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
      }
      CVPixelBufferRelease(buffer);
    }
  });
}

- (void)unregisterTexture:(int64_t)textureId {
  dispatch_async(_raster, ^{
    @autoreleasepool {
      NSObject<FlutterTexture>* texture = self->_textures[@(textureId)];
      if (!texture) return;
      [self->_textures removeObjectForKey:@(textureId)];
      if ([texture respondsToSelector:@selector(onTextureUnregistered:)]) {
        [texture onTextureUnregistered:texture];
      }
    }
  });
}

- (void)drain {
  dispatch_sync(_raster, ^{
  });
}

- (uint64_t)framesCopied {
  __block uint64_t copied = 0;
  dispatch_sync(_raster, ^{
    copied = self->_framesCopied;
  });
  return copied;
}

@end

namespace {

/// Invokes one method-channel call synchronously and returns its result.
id Invoke(LarimarPlugin* plugin, NSString* method, NSDictionary* arguments) {
  __block id captured = nil;
  FlutterMethodCall* call = [FlutterMethodCall methodCallWithMethodName:method
                                                             arguments:arguments];
  [plugin handleMethodCall:call
                    result:^(id result) {
                      captured = result;
                    }];
  return captured;
}

bool IsError(id result, NSString** code) {
  if (![result isKindOfClass:[FlutterError class]]) return false;
  if (code) *code = ((FlutterError*)result).code;
  return true;
}

/// Writes into the shared surface the way the Metal backend does each frame, so
/// the raster thread's borrow overlaps a real producer rather than idle memory.
void PaintTexture(uint64_t handle, NSInteger width, NSInteger height, int cycle) {
  id<MTLTexture> texture = (__bridge id<MTLTexture>)(void*)(uintptr_t)handle;
  if (!texture) return;
  const NSUInteger rowBytes = (NSUInteger)width * 4;
  NSMutableData* row = [NSMutableData dataWithLength:rowBytes];
  memset(row.mutableBytes, (int)(cycle & 0xff), rowBytes);
  [texture replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)width, 1)
             mipmapLevel:0
               withBytes:row.bytes
             bytesPerRow:rowBytes];
  (void)height;
}

int RunOneCycle(LarimarPlugin* plugin, int cycle) {
  id created = Invoke(plugin, @"create", @{@"width" : @(kWidth), @"height" : @(kHeight)});
  NSString* code = nil;
  if (IsError(created, &code)) {
    fprintf(stderr, "[host] create failed: %s\n", code.UTF8String);
    return 1;
  }
  NSDictionary* surface = created;
  NSNumber* textureId = surface[@"textureId"];

  // Frames, a resize under load, and suspend/resume all while the raster queue
  // is borrowing the surface. The resize is the interesting one: it swaps the
  // pixel buffer out from under an in-flight copy.
  for (int frame = 0; frame < 8; ++frame) {
    PaintTexture([surface[@"metalTexture"] unsignedLongLongValue],
                 [surface[@"width"] integerValue],
                 [surface[@"height"] integerValue], cycle + frame);
    Invoke(plugin, @"present", @{@"textureId" : textureId});
  }

  id resized = Invoke(plugin, @"resize", @{
    @"textureId" : textureId,
    @"width" : @(kWidth + ((cycle % 4) + 1) * 8),
    @"height" : @(kHeight + ((cycle % 4) + 1) * 8),
  });
  if (IsError(resized, &code)) {
    fprintf(stderr, "[host] resize failed: %s\n", code.UTF8String);
    return 1;
  }
  surface = resized;
  for (int frame = 0; frame < 8; ++frame) {
    PaintTexture([surface[@"metalTexture"] unsignedLongLongValue],
                 [surface[@"width"] integerValue],
                 [surface[@"height"] integerValue], cycle + frame);
    Invoke(plugin, @"present", @{@"textureId" : textureId});
  }

  // Application backgrounding arrives on the platform thread as both a channel
  // call and an app-delegate notification; the adapter must tolerate either
  // order while frames are still in flight.
  NSNotification* notification =
      [NSNotification notificationWithName:@"LarimarHarnessLifecycle" object:nil];
  Invoke(plugin, @"setSuspended", @{@"textureId" : textureId, @"suspended" : @YES});
  [plugin handleWillResignActive:notification];
  Invoke(plugin, @"present", @{@"textureId" : textureId});
  [plugin handleDidBecomeActive:notification];
  Invoke(plugin, @"setSuspended", @{@"textureId" : textureId, @"suspended" : @NO});
  Invoke(plugin, @"present", @{@"textureId" : textureId});

  Invoke(plugin, @"dispose", @{@"textureId" : textureId});
  // Disposal races the borrow deliberately: no drain here.
  return 0;
}

int RunCycles(LarimarPlugin* plugin, int cycles) {
  for (int cycle = 0; cycle < cycles; ++cycle) {
    // One pool per cycle, because that is what a platform thread's run loop
    // gives the adapter in a real application. Without it, the autoreleased
    // arrays the app-lifecycle handlers walk would hold every surface of the
    // whole run alive and the reclamation check below would prove nothing.
    @autoreleasepool {
      int status = RunOneCycle(plugin, cycle);
      if (status != 0) return status;
    }
  }
  return 0;
}

int ReportLifecycle(LarimarPlugin* plugin, RasterRegistry* registry, int cycles) {
  // The engine may still hold the last surface for a frame or two, so give the
  // raster queue a bounded chance to let go before reading the live count.
  NSDictionary* diagnostics = nil;
  for (int attempt = 0; attempt < 50; ++attempt) {
    @autoreleasepool {
      [registry drain];
      diagnostics = Invoke(plugin, @"diagnostics", @{});
    }
    if ([diagnostics[@"live"] unsignedLongLongValue] == 0) break;
    [NSThread sleepForTimeInterval:0.02];
  }

  const uint64_t created = [diagnostics[@"created"] unsignedLongLongValue];
  const uint64_t disposed = [diagnostics[@"disposed"] unsignedLongLongValue];
  const uint64_t live = [diagnostics[@"live"] unsignedLongLongValue];
  const uint64_t registered = [diagnostics[@"registered"] unsignedLongLongValue];
  const uint64_t bytes = [diagnostics[@"liveSurfaceBytes"] unsignedLongLongValue];
  const uint64_t frames = registry.framesCopied;

  printf("[host] cycles=%d created=%llu disposed=%llu registered=%llu live=%llu "
         "liveSurfaceBytes=%llu rasterCopies=%llu\n",
         cycles, created, disposed, registered, live, bytes, frames);

  int failures = 0;
  if (created != (uint64_t)cycles) {
    fprintf(stderr, "[host] expected %d creations, saw %llu\n", cycles, created);
    failures += 1;
  }
  if (created != disposed) {
    fprintf(stderr, "[host] %llu surfaces created but %llu disposed\n", created, disposed);
    failures += 1;
  }
  if (registered != 0 || live != 0 || bytes != 0) {
    fprintf(stderr, "[host] surfaces outlived the loop: registered=%llu live=%llu bytes=%llu\n",
            registered, live, bytes);
    failures += 1;
  }
  if (frames == 0) {
    fprintf(stderr, "[host] the raster queue never copied a surface; the loop proved nothing\n");
    failures += 1;
  }
  // `leaks --atExit` swallows the child's exit code, so the harness states its
  // own verdict on a line the runner can check.
  printf("[host] status=%s\n", failures == 0 ? "ok" : "failed");
  return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, const char* argv[]) {
  (void)argc;
  (void)argv;
  @autoreleasepool {
    const int cycles = IntegerFromEnvironment("LARIMAR_HOST_CYCLES", kDefaultCycles);

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
      // Virtualized runners have no Metal device. Say so plainly rather than
      // turning an absent GPU into a red build that reads like a race.
      printf("[host] no Metal device on this host; skipping the texture host harness\n");
      printf("[host] status=skipped-no-metal\n");
      return 0;
    }

    RasterRegistry* registry = [[RasterRegistry alloc] init];
    LarimarPlugin* plugin = [[LarimarPlugin alloc] initWithRegistry:registry];

    int status = RunCycles(plugin, cycles);
    if (status != 0) return status;
    return ReportLifecycle(plugin, registry, cycles);
  }
}
