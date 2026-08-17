/**
 * @file metal.mm
 * @brief Kantei Grade 3 (Brush) — Metal compute backend.
 *
 * Salvaged from renderer_metal.mm. The port is structural rather than
 * behavioural: the shader translation moved out to transpile.cpp (shared with
 * the GL backend), the renderer lost its constructor-throws error handling in
 * favour of the C result codes the boundary requires, and it now renders
 * whatever material the world holds instead of one baked in at construction.
 *
 * Metal is a graphics API, not a windowing system: this creates a system
 * default device and renders entirely offscreen, so it opens no window and
 * needs no host cooperation. That keeps it inside the §5 boundary rule, which
 * forbids SDL and windowing — not GPU APIs.
 */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../eshi_internal.h"
#include "transpile.h"

namespace {

struct MetalBackend {
    int32_t width;
    int32_t height;
    void*   device;
    void*   queue;
    void*   pipeline;
    void*   output_texture;
    void*   input_texture;
    void*   readback;
    size_t  uniform_capacity;
};

int metal_available(void) {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        return device != nil ? 1 : 0;
    }
}

EshiBackend* metal_create(int32_t width, int32_t height,
                          const char* source_path, const char* /*package_path*/) {
    if (!source_path) {
        std::fprintf(stderr,
                     "[eshi/metal] material has no source_path; GPU tiers cannot "
                     "execute a compiled-in CPU shader\n");
        return NULL;
    }

    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            std::fprintf(stderr, "[eshi/metal] no Metal device\n");
            return NULL;
        }

        /*
         * The uniform block is bound as a flat float array. Its size is not
         * known until a material is set, so the kernel is compiled against the
         * largest block the backend supports and the unused tail is ignored.
         */
        const int kUniformFloats = 64;

        std::string source, error;
        if (!eshi::transpile::build_program(source_path,
                                            eshi::transpile::kTargetMsl,
                                            kUniformFloats, &source, &error)) {
            std::fprintf(stderr, "[eshi/metal] %s\n", error.c_str());
            return NULL;
        }

        NSError* ns_error = nil;
        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        id<MTLLibrary> library =
            [device newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()]
                                 options:options
                                   error:&ns_error];
        if (!library) {
            std::fprintf(stderr, "[eshi/metal] shader compilation failed: %s\n",
                         [[ns_error localizedDescription] UTF8String]);
            return NULL;
        }

        id<MTLFunction> function = [library newFunctionWithName:@"eshi_main"];
        if (!function) {
            std::fprintf(stderr, "[eshi/metal] entry point eshi_main not found\n");
            return NULL;
        }

        id<MTLComputePipelineState> pipeline =
            [device newComputePipelineStateWithFunction:function error:&ns_error];
        if (!pipeline) {
            std::fprintf(stderr, "[eshi/metal] pipeline creation failed: %s\n",
                         [[ns_error localizedDescription] UTF8String]);
            return NULL;
        }

        MTLTextureDescriptor* out_desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                               width:(NSUInteger)width
                                                              height:(NSUInteger)height
                                                           mipmapped:NO];
        out_desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        id<MTLTexture> output_texture = [device newTextureWithDescriptor:out_desc];

        /* A one-pixel black texture keeps iChannel0 bound for shaders that ignore it. */
        MTLTextureDescriptor* in_desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                               width:1
                                                              height:1
                                                           mipmapped:NO];
        in_desc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> input_texture = [device newTextureWithDescriptor:in_desc];
        const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        [input_texture replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                         mipmapLevel:0
                           withBytes:black
                         bytesPerRow:4 * sizeof(float)];

        id<MTLBuffer> readback =
            [device newBufferWithLength:(NSUInteger)width * (NSUInteger)height * 4
                                options:MTLResourceStorageModeShared];

        MetalBackend* backend = new MetalBackend();
        backend->width = width;
        backend->height = height;
        backend->device = (__bridge_retained void*)device;
        backend->queue = (__bridge_retained void*)[device newCommandQueue];
        backend->pipeline = (__bridge_retained void*)pipeline;
        backend->output_texture = (__bridge_retained void*)output_texture;
        backend->input_texture = (__bridge_retained void*)input_texture;
        backend->readback = (__bridge_retained void*)readback;
        backend->uniform_capacity = (size_t)kUniformFloats * sizeof(float);

        std::printf("[eshi/metal] %s\n", [[device name] UTF8String]);
        return reinterpret_cast<EshiBackend*>(backend);
    }
}

void metal_destroy(EshiBackend* handle) {
    MetalBackend* backend = reinterpret_cast<MetalBackend*>(handle);
    if (!backend) return;
    if (backend->device) CFRelease(backend->device);
    if (backend->queue) CFRelease(backend->queue);
    if (backend->pipeline) CFRelease(backend->pipeline);
    if (backend->output_texture) CFRelease(backend->output_texture);
    if (backend->input_texture) CFRelease(backend->input_texture);
    if (backend->readback) CFRelease(backend->readback);
    delete backend;
}

EshiResult metal_render(EshiBackend* handle,
                        uint8_t* pixels, int32_t stride, float time,
                        EshiShaderFn /*cpu_shader*/,
                        const void* uniforms, size_t uniform_size) {
    MetalBackend* backend = reinterpret_cast<MetalBackend*>(handle);
    if (!backend) return ESHI_ERR_INVALID;
    if (uniform_size > backend->uniform_capacity) return ESHI_ERR_LIMIT;

    id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)backend->queue;
    id<MTLComputePipelineState> pipeline =
        (__bridge id<MTLComputePipelineState>)backend->pipeline;
    id<MTLTexture> output_texture = (__bridge id<MTLTexture>)backend->output_texture;
    id<MTLTexture> input_texture = (__bridge id<MTLTexture>)backend->input_texture;
    id<MTLBuffer> readback = (__bridge id<MTLBuffer>)backend->readback;

    const int32_t width = backend->width;
    const int32_t height = backend->height;

    @autoreleasepool {
        id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];

        [encoder setComputePipelineState:pipeline];
        [encoder setBytes:&time length:sizeof(float) atIndex:0];
        if (uniforms && uniform_size > 0) {
            [encoder setBytes:uniforms length:uniform_size atIndex:1];
        }
        [encoder setTexture:output_texture atIndex:0];
        [encoder setTexture:input_texture atIndex:1];

        [encoder dispatchThreads:MTLSizeMake((NSUInteger)width, (NSUInteger)height, 1)
           threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
        [encoder endEncoding];

        id<MTLBlitCommandEncoder> blit = [command_buffer blitCommandEncoder];
        [blit copyFromTexture:output_texture
                  sourceSlice:0
                  sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake((NSUInteger)width, (NSUInteger)height, 1)
                     toBuffer:readback
            destinationOffset:0
       destinationBytesPerRow:(NSUInteger)width * 4
     destinationBytesPerImage:(NSUInteger)width * (NSUInteger)height * 4];
        [blit endEncoding];

        [command_buffer commit];
        [command_buffer waitUntilCompleted];

        const uint8_t* source = static_cast<const uint8_t*>([readback contents]);
        const size_t source_stride = (size_t)width * 4;
        for (int32_t y = 0; y < height; ++y) {
            std::memcpy(pixels + (size_t)y * (size_t)stride,
                        source + (size_t)y * source_stride,
                        source_stride);
        }
    }

    return ESHI_OK;
}

const EshiBackendVTable kMetalVTable = {
    "metal",
    metal_available,
    metal_create,
    metal_destroy,
    metal_render,
};

} /* namespace */

extern "C" const EshiBackendVTable* eshi__backend_metal(void) { return &kMetalVTable; }
