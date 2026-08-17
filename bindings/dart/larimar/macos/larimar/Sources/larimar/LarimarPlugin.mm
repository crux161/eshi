#import <larimar/LarimarPlugin.h>

#import <CoreVideo/CoreVideo.h>
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
  if (_cache) CFRelease(_cache);
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
                                 MTLTextureUsageShaderWrite),
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
  [_surfaceLock unlock];
  return buffer;
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
        [_registry textureFrameAvailable:texture.textureId];
      }
      result(nil);
    }
  } else if ([call.method isEqualToString:@"setSuspended"]) {
    LarimarTexture* texture = [self textureFromArguments:arguments result:result];
    if (texture) {
      texture.suspended = [arguments[@"suspended"] boolValue];
      result(nil);
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

- (NSDictionary*)descriptionForTexture:(LarimarTexture*)texture {
  return @{
    @"textureId" : @(texture.textureId),
    @"metalTexture" : @(texture.metalTextureHandle),
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
    texture.suspended = YES;
    [_registry unregisterTexture:texture.textureId];
  }
  [_textures removeAllObjects];
}

@end
