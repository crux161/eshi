#import "renderer_metal.h"
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <sys/stat.h>

// --- Helper to convert C++ string to NSString ---
static NSString* toNSString(const std::string& str) {
    return [NSString stringWithUTF8String:str.c_str()];
}

static std::string resolvePath(std::string path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) == 0) return path;
    std::string up = "../" + path;
    if (stat(up.c_str(), &buffer) == 0) return up;
    if (path.substr(0, 3) == "../") {
        std::string stripped = path.substr(3);
        if (stat(stripped.c_str(), &buffer) == 0) return stripped;
    }
    return "";
}

static std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

static std::string readFile(std::string path) {
    std::string cleanPath = resolvePath(path);
    if (cleanPath.empty()) {
        std::cerr << "[Metal ERROR] File not found: " << path << std::endl;
        exit(1);
    }

    std::ifstream f(cleanPath);
    std::string content, line;
    while(std::getline(f, line)) {
        if(line.find("#include") != std::string::npos) continue;
        if(line.find("#pragma") != std::string::npos) continue;
        if(line.find("using namespace") != std::string::npos) continue;
        if(line.find("extern") != std::string::npos) continue; 
        
        line = replaceAll(line, "inline ", "");
        line = replaceAll(line, "glsl::", "");
        
        line = replaceAll(line, "float&", "thread float&");
        line = replaceAll(line, "float &", "thread float& ");
        line = replaceAll(line, "vec2&", "thread vec2&");
        line = replaceAll(line, "vec2 &", "thread vec2& ");
        line = replaceAll(line, "vec3&", "thread vec3&");
        line = replaceAll(line, "vec3 &", "thread vec3& ");
        line = replaceAll(line, "vec4&", "thread vec4&");
        line = replaceAll(line, "vec4 &", "thread vec4& ");

        line = replaceAll(line, "if (iChannel0.data == nullptr)", "if (false)");

        line = std::regex_replace(line, std::regex("\\.([xyzw]{1,4})\\(\\)"), ".$1");
        line = std::regex_replace(line, std::regex("SHADER_CTX\\s+vec4\\s+tanh\\(vec4\\s+v\\)\\s*\\{"), "vec4 __dummy_tanh(vec4 v) {");
        line = replaceAll(line, "::tanhf", "tanh");

        line = replaceAll(line, "texture(iChannel0,", "iChannel0.sample(smp,");
        line = replaceAll(line, "texture(::iChannel0,", "iChannel0.sample(smp,");

        if (line.find("mainImage") != std::string::npos) {
            line = std::regex_replace(line, std::regex("\\)\\s*\\{"), ", texture2d<float, access::sample> iChannel0, sampler smp) {");
        }

        content += line + "\n";
    }
    content = replaceAll(content, "\nconst ", "\nconstant ");
    return content;
}

MetalRenderer::MetalRenderer(int w, int h, const char* shaderPath, float* texData, int texW, int texH) 
    : width(w), height(h) {
    
    // 1. Initialize Metal Device & Command Queue
    id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
    if (!mtlDevice) {
        std::cerr << "[FATAL] Metal is not supported on this device." << std::endl;
        exit(1);
    }
    
    device = (__bridge_retained void*)mtlDevice;
    
    id<MTLCommandQueue> mtlCommandQueue = [mtlDevice newCommandQueue];
    commandQueue = (__bridge_retained void*)mtlCommandQueue;

    std::cout << "[Metal] Initializing on " << [[mtlDevice name] UTF8String] << std::endl;

    // 2. Read and Transform Shader (Similar to your OpenGL readFile approach)
    std::string userCode = readFile(shaderPath); 
    
    // Wrap the user code in the Metal Shading Language (MSL) boilerplate
    std::string mslSource = 
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "typedef float2 vec2;\n"
        "typedef float3 vec3;\n"
        "typedef float4 vec4;\n"
        "typedef float2x2 mat2;\n"
        "typedef float3x3 mat3;\n"
        "typedef float4x4 mat4;\n"
        "// --- MSL Compatibility Wrapper ---\n"
        "#define SUMI_CTX \n" 
        "#define SHADER_CTX \n"
        "#define M_PI 3.14159265359\n"
        "#define sinf sin\n"
        "#define cosf cos\n"
        "#define tanf tan\n"
        "#define asinf asin\n"
        "#define acosf acos\n"
        "#define atanf atan\n"
        "#define atan2f atan\n"
        "#define powf pow\n"
        "#define expf exp\n"
        "#define logf log\n"
        "#define sqrtf sqrt\n"
        "#define fabsf abs\n"
        "#define floorf floor\n"
        "#define ceilf ceil\n"
        "#define modf fmod\n"
        "#define fminf min\n"
        "#define fmaxf max\n"
        "#define glsl_core_h\n"
        "constexpr sampler smp(coord::normalized, address::repeat, filter::linear);\n"
        + userCode + "\n"
        "kernel void computeMain(texture2d<float, access::write> outTexture [[texture(0)]],\n"
        "                        texture2d<float, access::sample> iChannel0 [[texture(1)]],\n"
        "                        constant float* time [[buffer(0)]],\n"
        "                        uint2 gid [[thread_position_in_grid]]) {\n"
        "    if (gid.x >= outTexture.get_width() || gid.y >= outTexture.get_height()) return;\n"
        "    vec2 iResolution = vec2(outTexture.get_width(), outTexture.get_height());\n"
        "    vec2 fragCoord = vec2(gid.x, outTexture.get_height() - 1 - gid.y);\n"
        "    vec4 fragColor = vec4(0.0);\n"
        "    mainImage(fragColor, fragCoord, iResolution, *time, iChannel0, smp);\n"
        "    outTexture.write(fragColor, gid);\n"
        "}\n";

    // 3. Compile the Library at Runtime
    NSError* error = nil;
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    id<MTLLibrary> library = [mtlDevice newLibraryWithSource:toNSString(mslSource) 
                                                     options:options 
                                                       error:&error];
    if (!library) {
        std::cerr << "[Metal ERROR] Shader compilation failed:\n" 
                  << [[error localizedDescription] UTF8String] << std::endl;
        exit(1);
    }

    // 4. Create Compute Pipeline State
    id<MTLFunction> computeFunction = [library newFunctionWithName:@"computeMain"];
    id<MTLComputePipelineState> pso = [mtlDevice newComputePipelineStateWithFunction:computeFunction error:&error];
    pipelineState = (__bridge_retained void*)pso;

    // 5. Setup Textures and Output Buffers
    // For offscreen rendering (like your video encoder pipeline), you need a texture to write to
    // and a buffer to copy the pixels back to the CPU (pixelBuffer in renderFrame).
    
    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm 
                                                                                       width:width 
                                                                                      height:height 
                                                                                   mipmapped:NO];
    texDesc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    id<MTLTexture> mtlOutputTexture = [mtlDevice newTextureWithDescriptor:texDesc];
    this->outputTexture = (__bridge_retained void*)mtlOutputTexture;
    
    // Setup iChannel0 user texture if provided (similar to OpenGL init)
    if (texData) {
        // Create an input MTLTexture and copy texData into it
    }
    
    // Allocate CPU-accessible buffer to read results back
    NSUInteger bufferSize = width * height * 4; // RGBA 8-bit
    id<MTLBuffer> readbackBuffer = [mtlDevice newBufferWithLength:bufferSize options:MTLResourceStorageModeShared];
    outputBuffer = (__bridge_retained void*)readbackBuffer;
}

MetalRenderer::~MetalRenderer() {
    // Release ARC handles
    if (device) CFRelease(device);
    if (commandQueue) CFRelease(commandQueue);
    if (pipelineState) CFRelease(pipelineState);
    if (texture) CFRelease(texture);
    if (outputTexture) CFRelease(outputTexture);
    if (outputBuffer) CFRelease(outputBuffer);
}

void MetalRenderer::renderFrame(uint8_t* pixelBuffer, int stride, float time) {
    id<MTLCommandQueue> mtlQueue = (__bridge id<MTLCommandQueue>)commandQueue;
    id<MTLComputePipelineState> pso = (__bridge id<MTLComputePipelineState>)pipelineState;
    id<MTLBuffer> mtlOutputBuffer = (__bridge id<MTLBuffer>)outputBuffer;
    id<MTLTexture> mtlOutputTexture = (__bridge id<MTLTexture>)outputTexture;
    // ... get your texture ...

    @autoreleasepool {
        id<MTLCommandBuffer> commandBuffer = [mtlQueue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
        
        [encoder setComputePipelineState:pso];
        
        // Bind uniforms and textures
        [encoder setBytes:&time length:sizeof(float) atIndex:0];
        [encoder setTexture:mtlOutputTexture atIndex:0];
        // [encoder setTexture:mtlInputTexture atIndex:1];

        // Dispatch Threads
        MTLSize gridSize = MTLSizeMake(width, height, 1);
        // Calculate threadgroups based on max threads
        MTLSize threadgroupSize = MTLSizeMake(16, 16, 1); 
        
        [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];
        [encoder endEncoding];

        // Instruct Metal to copy the resulting texture data to the CPU-accessible buffer
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        [blitEncoder copyFromTexture:mtlOutputTexture
                         sourceSlice:0
                         sourceLevel:0
                        sourceOrigin:MTLOriginMake(0, 0, 0)
                          sourceSize:MTLSizeMake(width, height, 1)
                            toBuffer:mtlOutputBuffer
                   destinationOffset:0
              destinationBytesPerRow:width * 4
            destinationBytesPerImage:width * height * 4];
        [blitEncoder endEncoding];

        // Submit and wait
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
        
        // Copy the data from the Metal buffer into the SDL/Encoder pixelBuffer
        memcpy(pixelBuffer, [mtlOutputBuffer contents], width * height * 4);
    }
}
