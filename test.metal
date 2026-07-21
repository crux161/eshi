#include <metal_stdlib>
using namespace metal;
struct ShaderContext {
    texture2d<float, access::sample> iChannel0;
    constant float foo = 4.0f;
    void test() {}
};
kernel void computeMain() {}
