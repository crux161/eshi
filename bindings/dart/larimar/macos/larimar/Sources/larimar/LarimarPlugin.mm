#import <larimar/LarimarPlugin.h>

#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/ImageIO.h>
#import <Metal/Metal.h>

namespace {

static NSString* const kChannelName = @"dev.larimar/texture";
static const NSInteger kMaximumTextureDimension = 16384;

FlutterError* LarimarError(NSString* code, NSString* message) {
  return [FlutterError errorWithCode:code message:message details:nil];
}

}  // namespace

@interface LarimarTexture : NSObject <FlutterTexture>

@property(nonatomic, readonly) int64_t textureId;
@property(nonatomic, readonly) NSInteger width;
@property(nonatomic, readonly) NSInteger height;
@property(nonatomic, readonly) uint64_t metalTextureHandle;
@property(nonatomic, readonly) uint64_t pixelBufferHandle;
@property(nonatomic, readonly) uint64_t surfaceBytes;
@property(nonatomic, readonly) uint64_t copies;
@property(atomic, assign, getter=isSuspended) BOOL suspended;

- (instancetype)initWithDevice:(id<MTLDevice>)device
                           cache:(CVMetalTextureCacheRef)cache
                           width:(NSInteger)width
                          height:(NSInteger)height
                           error:(FlutterError**)error;
- (BOOL)resizeToWidth:(NSInteger)width
                height:(NSInteger)height
                 error:(FlutterError**)error;
- (void)setTextureId:(int64_t)textureId;

@end

@implementation LarimarTexture {
  id<MTLDevice> _device;
  CVMetalTextureCacheRef _cache;
  CVPixelBufferRef _pixelBuffer;
  CVMetalTextureRef _cvMetalTexture;
  NSLock* _surfaceLock;
  NSInteger _width;
  NSInteger _height;
  int64_t _textureId;
  uint64_t _copies;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device
                           cache:(CVMetalTextureCacheRef)cache
                           width:(NSInteger)width
                          height:(NSInteger)height
                           error:(FlutterError**)error {
  self = [super init];
  if (self) {
    _device = device;
    _cache = (CVMetalTextureCacheRef)CFRetain(cache);
    _surfaceLock = [[NSLock alloc] init];
    if (![self resizeToWidth:width height:height error:error]) {
      return nil;
    }
  }
  return self;
}

- (void)dealloc {
  [_surfaceLock lock];
  if (_cvMetalTexture) CFRelease(_cvMetalTexture);
  if (_pixelBuffer) CVPixelBufferRelease(_pixelBuffer);
  [_surfaceLock unlock];
  if (_cache) {
    // Apple's documented maintenance step: a texture cache may hold entries for
    // buffers nothing else references. Measured against the diagnostics harness
    // this release path is already clean without it, so this is not a fix for a
    // leak we found — it keeps the discipline explicit instead of depending on
    // CoreVideo continuing to drop the entry eagerly.
    CVMetalTextureCacheFlush(_cache, 0);
    CFRelease(_cache);
  }
}

- (NSInteger)width {
  [_surfaceLock lock];
  NSInteger value = _width;
  [_surfaceLock unlock];
  return value;
}

- (NSInteger)height {
  [_surfaceLock lock];
  NSInteger value = _height;
  [_surfaceLock unlock];
  return value;
}

- (int64_t)textureId {
  return _textureId;
}

- (void)setTextureId:(int64_t)textureId {
  _textureId = textureId;
}

- (uint64_t)metalTextureHandle {
  [_surfaceLock lock];
  id<MTLTexture> texture =
      _cvMetalTexture ? CVMetalTextureGetTexture(_cvMetalTexture) : nil;
  uint64_t handle = (uint64_t)(uintptr_t)(__bridge void*)texture;
  [_surfaceLock unlock];
  return handle;
}

- (uint64_t)pixelBufferHandle {
  [_surfaceLock lock];
  uint64_t handle = (uint64_t)(uintptr_t)_pixelBuffer;
  [_surfaceLock unlock];
  return handle;
}

- (uint64_t)surfaceBytes {
  [_surfaceLock lock];
  uint64_t bytes = _pixelBuffer ? (uint64_t)CVPixelBufferGetDataSize(_pixelBuffer) : 0;
  [_surfaceLock unlock];
  return bytes;
}

- (BOOL)resizeToWidth:(NSInteger)width
                height:(NSInteger)height
                 error:(FlutterError**)error {
  NSDictionary* attributes = @{
    (id)kCVPixelBufferIOSurfacePropertiesKey : @{},
    (id)kCVPixelBufferMetalCompatibilityKey : @YES,
  };
  CVPixelBufferRef pixelBuffer = nil;
  CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, width, height,
                                        kCVPixelFormatType_32BGRA,
                                        (__bridge CFDictionaryRef)attributes,
                                        &pixelBuffer);
  if (status != kCVReturnSuccess || !pixelBuffer) {
    if (error) {
      *error = LarimarError(@"surface-allocation",
                            [NSString stringWithFormat:
                                @"CVPixelBufferCreate failed (%d).", status]);
    }
    return NO;
  }

  NSDictionary* textureAttributes = @{
    (id)kCVMetalTextureUsage : @(MTLTextureUsageShaderRead |
                                 MTLTextureUsageShaderWrite |
                                 MTLTextureUsageRenderTarget),
  };
  CVMetalTextureRef cvTexture = nil;
  status = CVMetalTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, _cache, pixelBuffer,
      (__bridge CFDictionaryRef)textureAttributes,
      MTLPixelFormatBGRA8Unorm, width, height, 0, &cvTexture);
  if (status != kCVReturnSuccess || !cvTexture ||
      !CVMetalTextureGetTexture(cvTexture)) {
    if (cvTexture) CFRelease(cvTexture);
    CVPixelBufferRelease(pixelBuffer);
    if (error) {
      *error = LarimarError(@"metal-texture-allocation",
                            [NSString stringWithFormat:
                                @"CVMetalTexture creation failed (%d).", status]);
    }
    return NO;
  }

  [_surfaceLock lock];
  CVMetalTextureRef oldTexture = _cvMetalTexture;
  CVPixelBufferRef oldBuffer = _pixelBuffer;
  _cvMetalTexture = cvTexture;
  _pixelBuffer = pixelBuffer;
  _width = width;
  _height = height;
  [_surfaceLock unlock];

  // copyPixelBuffer returns an owning reference, so a raster-thread borrower
  // can safely finish consuming the old surface after this swap.
  if (oldTexture) CFRelease(oldTexture);
  if (oldBuffer) CVPixelBufferRelease(oldBuffer);
  CVMetalTextureCacheFlush(_cache, 0);
  return YES;
}

- (CVPixelBufferRef)copyPixelBuffer {
  [_surfaceLock lock];
  CVPixelBufferRef buffer =
      _pixelBuffer ? CVPixelBufferRetain(_pixelBuffer) : nil;
  // Counted under the same lock the surface swap holds, because this runs on
  // the raster thread. It is the only signal that distinguishes "frames are
  // being produced" from "frames are being *composited*": the engine calls this
  // when, and only when, it is drawing the texture layer. A view that presents
  // frames while this stays at zero is on screen as a hole.
  if (buffer) _copies += 1;
  [_surfaceLock unlock];
  return buffer;
}

- (uint64_t)copies {
  [_surfaceLock lock];
  uint64_t value = _copies;
  [_surfaceLock unlock];
  return value;
}

/// Writes the surface itself to a PNG, bypassing Flutter's compositor entirely.
///
/// This answers a question no widget test can: whether the pixels Larimar
/// submitted are in the surface Flutter was handed. A composited capture that
/// comes back empty is either an empty surface or a layer that was not drawn,
/// and those have completely different causes.
- (BOOL)writePNGTo:(NSString*)path error:(FlutterError**)error {
  [_surfaceLock lock];
  CVPixelBufferRef buffer = _pixelBuffer ? CVPixelBufferRetain(_pixelBuffer) : nil;
  [_surfaceLock unlock];
  if (!buffer) {
    if (error) *error = LarimarError(@"no-surface", @"The view has no surface to capture.");
    return NO;
  }

  BOOL ok = NO;
  if (CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly) == kCVReturnSuccess) {
    const size_t width = CVPixelBufferGetWidth(buffer);
    const size_t height = CVPixelBufferGetHeight(buffer);
    const size_t stride = CVPixelBufferGetBytesPerRow(buffer);
    void* base = CVPixelBufferGetBaseAddress(buffer);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        base, width, height, 8, stride, space,
        kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little);
    CGImageRef image = context ? CGBitmapContextCreateImage(context) : NULL;
    if (image) {
      NSURL* url = [NSURL fileURLWithPath:path];
      CGImageDestinationRef destination =
          CGImageDestinationCreateWithURL((__bridge CFURLRef)url,
                                          CFSTR("public.png"), 1, NULL);
      if (destination) {
        CGImageDestinationAddImage(destination, image, NULL);
        ok = CGImageDestinationFinalize(destination);
        CFRelease(destination);
      }
      CGImageRelease(image);
    }
    if (context) CGContextRelease(context);
    CGColorSpaceRelease(space);
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
  }
  CVPixelBufferRelease(buffer);
  if (!ok && error) {
    *error = LarimarError(@"capture-failed", @"Could not encode the surface as a PNG.");
  }
  return ok;
}

- (void)onTextureUnregistered:(NSObject<FlutterTexture>*)texture {
  self.suspended = YES;
}

@end

@implementation LarimarPlugin {
  id<FlutterTextureRegistry> _registry;
  id<MTLDevice> _device;
  CVMetalTextureCacheRef _cache;
  NSMutableDictionary<NSNumber*, LarimarTexture*>* _textures;
  // Weakly held, so a texture the engine is still borrowing on the raster
  // thread stays visible to `diagnostics` after the plugin has released it.
  // That distinction is the whole point: `registered` returning to zero only
  // says the plugin let go, while `live` returning to zero says the surface
  // and its IOSurface were actually reclaimed.
  NSHashTable<LarimarTexture*>* _liveTextures;
  uint64_t _created;
  uint64_t _resized;
  uint64_t _presented;
  uint64_t _disposed;
  uint64_t _suspensions;
  /// Raster-thread borrows charged by surfaces the plugin has already released.
  uint64_t _retiredCopies;
}

+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  FlutterMethodChannel* channel =
      [FlutterMethodChannel methodChannelWithName:kChannelName
                                  binaryMessenger:registrar.messenger];
  LarimarPlugin* plugin = [[LarimarPlugin alloc] initWithRegistry:registrar.textures];
  [registrar addMethodCallDelegate:plugin channel:channel];
  [registrar addApplicationDelegate:plugin];
}

- (instancetype)initWithRegistry:(id<FlutterTextureRegistry>)registry {
  self = [super init];
  if (self) {
    _registry = registry;
    _device = MTLCreateSystemDefaultDevice();
    _textures = [[NSMutableDictionary alloc] init];
    _liveTextures = [NSHashTable weakObjectsHashTable];
    if (_device) {
      CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, _device, nil, &_cache);
    }
  }
  return self;
}

- (void)dealloc {
  if (_cache) CFRelease(_cache);
}

- (void)handleMethodCall:(FlutterMethodCall*)call result:(FlutterResult)result {
  NSDictionary* arguments =
      [call.arguments isKindOfClass:[NSDictionary class]] ? call.arguments : @{};

  if ([call.method isEqualToString:@"create"]) {
    [self createTexture:arguments result:result];
  } else if ([call.method isEqualToString:@"resize"]) {
    [self resizeTexture:arguments result:result];
  } else if ([call.method isEqualToString:@"present"]) {
    LarimarTexture* texture = [self textureFromArguments:arguments result:result];
    if (texture) {
      if (!texture.isSuspended) {
        _presented += 1;
        [_registry textureFrameAvailable:texture.textureId];
      }
      result(nil);
    }
  } else if ([call.method isEqualToString:@"setSuspended"]) {
    LarimarTexture* texture = [self textureFromArguments:arguments result:result];
    if (texture) {
      _suspensions += 1;
      texture.suspended = [arguments[@"suspended"] boolValue];
      result(nil);
    }
  } else if ([call.method isEqualToString:@"diagnostics"]) {
    result([self diagnostics]);
  } else if ([call.method isEqualToString:@"captureSurface"]) {
    LarimarTexture* texture = [self textureFromArguments:arguments result:result];
    if (texture) {
      NSString* path = arguments[@"path"];
      if (![path isKindOfClass:[NSString class]]) {
        result(LarimarError(@"invalid-path", @"A destination path is required."));
        return;
      }
      FlutterError* error = nil;
      result([texture writePNGTo:path error:&error] ? (id)path : (id)error);
    }
  } else if ([call.method isEqualToString:@"dispose"]) {
    id value = arguments[@"textureId"];
    if (![value isKindOfClass:[NSNumber class]]) {
      result(LarimarError(@"invalid-texture", @"A numeric textureId is required."));
      return;
    }
    NSNumber* textureId = value;
    LarimarTexture* texture = _textures[textureId];
    if (texture) {
      _disposed += 1;
      _retiredCopies += texture.copies;
      texture.suspended = YES;
      [_textures removeObjectForKey:textureId];
      [_registry unregisterTexture:texture.textureId];
    }
    result(nil);
  } else {
    result(FlutterMethodNotImplemented);
  }
}

- (void)createTexture:(NSDictionary*)arguments result:(FlutterResult)result {
  if (!_device || !_cache) {
    result(LarimarError(@"metal-unavailable", @"No Metal device is available."));
    return;
  }
  NSInteger width = [arguments[@"width"] integerValue];
  NSInteger height = [arguments[@"height"] integerValue];
  if (![self dimensionsAreValidWidth:width height:height result:result]) return;

  FlutterError* error = nil;
  LarimarTexture* texture = [[LarimarTexture alloc] initWithDevice:_device
                                                               cache:_cache
                                                               width:width
                                                              height:height
                                                               error:&error];
  if (!texture) {
    result(error);
    return;
  }
  int64_t textureId = [_registry registerTexture:texture];
  if (textureId == 0) {
    result(LarimarError(@"texture-registration",
                        @"Flutter rejected the external texture."));
    return;
  }
  [texture setTextureId:textureId];
  _textures[@(textureId)] = texture;
  [_liveTextures addObject:texture];
  _created += 1;
  result([self descriptionForTexture:texture]);
}

- (void)resizeTexture:(NSDictionary*)arguments result:(FlutterResult)result {
  LarimarTexture* texture = [self textureFromArguments:arguments result:result];
  if (!texture) return;
  NSInteger width = [arguments[@"width"] integerValue];
  NSInteger height = [arguments[@"height"] integerValue];
  if (![self dimensionsAreValidWidth:width height:height result:result]) return;
  if (texture.width == width && texture.height == height) {
    result([self descriptionForTexture:texture]);
    return;
  }
  FlutterError* error = nil;
  if (![texture resizeToWidth:width height:height error:&error]) {
    result(error);
    return;
  }
  _resized += 1;
  result([self descriptionForTexture:texture]);
}

- (LarimarTexture*)textureFromArguments:(NSDictionary*)arguments
                                  result:(FlutterResult)result {
  id value = arguments[@"textureId"];
  if (![value isKindOfClass:[NSNumber class]]) {
    result(LarimarError(@"invalid-texture", @"A numeric textureId is required."));
    return nil;
  }
  NSNumber* textureId = value;
  LarimarTexture* texture = _textures[textureId];
  if (!texture) {
    result(LarimarError(@"unknown-texture", @"The EshiView texture is disposed."));
  }
  return texture;
}

- (BOOL)dimensionsAreValidWidth:(NSInteger)width
                          height:(NSInteger)height
                          result:(FlutterResult)result {
  if (width < 1 || height < 1 || width > kMaximumTextureDimension ||
      height > kMaximumTextureDimension) {
    result(LarimarError(@"invalid-size",
                        @"Texture dimensions must be between 1 and 16384."));
    return NO;
  }
  return YES;
}

// The leak gate. Every counter is mutated on the platform thread only, which is
// also where Flutter requires texture registration and unregistration to
// happen, so no lock is needed here — see the thread-affinity rules in
// docs/larimar/NATIVE_CONTRACT.md.
- (NSDictionary*)diagnostics {
  NSArray<LarimarTexture*>* live = _liveTextures.allObjects;
  uint64_t bytes = 0;
  uint64_t copies = _retiredCopies;
  for (LarimarTexture* texture in live) {
    bytes += texture.surfaceBytes;
    copies += texture.copies;
  }
  return @{
    @"created" : @(_created),
    @"resized" : @(_resized),
    @"presented" : @(_presented),
    @"disposed" : @(_disposed),
    @"suspensions" : @(_suspensions),
    @"registered" : @(_textures.count),
    @"live" : @(live.count),
    @"liveSurfaceBytes" : @(bytes),
    @"copies" : @(copies),
    @"metalAvailable" : @(_device != nil && _cache != nil),
  };
}

- (NSDictionary*)descriptionForTexture:(LarimarTexture*)texture {
  return @{
    @"textureId" : @(texture.textureId),
    @"metalTexture" : @(texture.metalTextureHandle),
    @"pixelBuffer" : @(texture.pixelBufferHandle),
    @"width" : @(texture.width),
    @"height" : @(texture.height),
  };
}

- (void)handleWillResignActive:(NSNotification*)notification {
  for (LarimarTexture* texture in _textures.allValues) texture.suspended = YES;
}

- (void)handleDidBecomeActive:(NSNotification*)notification {
  for (LarimarTexture* texture in _textures.allValues) texture.suspended = NO;
}

- (void)handleWillTerminate:(NSNotification*)notification {
  NSArray<NSNumber*>* textureIds = _textures.allKeys;
  for (NSNumber* textureId in textureIds) {
    LarimarTexture* texture = _textures[textureId];
    _disposed += 1;
    texture.suspended = YES;
    [_registry unregisterTexture:texture.textureId];
  }
  [_textures removeAllObjects];
}

@end
