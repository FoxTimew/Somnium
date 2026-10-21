// Isolates hosted Metal shader-validation failures from Somnium, SDL and shadercross.
// A diagnostic only: this native SDK program never replaces the required D5 render.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <array>
#include <cmath>
#include <cstdio>

int main() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            std::puts("No Metal device");
            return 77;
        }
        NSLog(@"Independent device: %@", device.name);
        NSError* error = nil;
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.languageVersion = MTLLanguageVersion2_3;
        NSString* source = @"#include <metal_stdlib>\n"
                            "using namespace metal;\n"
                            "kernel void probe(texture2d<float> tex [[texture(0)]], "
                            "sampler nearest [[sampler(0)]], device float4* out [[buffer(0)]], "
                            "uint i [[thread_position_in_grid]]) {\n"
                            "  out[i] = tex.sample(nearest, "
                            "(float2(i % 2, i / 2) + 0.5f) / 2.0f, level(0));\n}\n";
        id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
        id<MTLFunction> function = [library newFunctionWithName:@"probe"];
        if (!function) {
            NSLog(@"Shader failed: %@", error);
            return 1;
        }
        id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function
                                                                                     error:&error];
        if (!pipeline) {
            NSLog(@"Pipeline failed: %@", error);
            return 1;
        }
        MTLTextureDescriptor* description =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                               width:2
                                                              height:2
                                                           mipmapped:NO];
        description.usage = MTLTextureUsageShaderRead;
        description.storageMode = MTLStorageModeShared;
        id<MTLTexture> texture = [device newTextureWithDescriptor:description];
        constexpr std::array<unsigned char, 16> pixels{255, 0, 0,   255, 0,   255, 0, 255,
                                                       0,   0, 255, 255, 255, 255, 0, 255};
        MTLSamplerDescriptor* samplerDescription = [MTLSamplerDescriptor new];
        samplerDescription.minFilter = MTLSamplerMinMagFilterNearest;
        samplerDescription.magFilter = MTLSamplerMinMagFilterNearest;
        id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:samplerDescription];
        id<MTLBuffer> output = [device newBufferWithLength:sizeof(float) * pixels.size()
                                                   options:MTLResourceStorageModeShared];
        id<MTLCommandQueue> queue = [device newCommandQueue];
        id<MTLCommandBuffer> command = [queue commandBuffer];
        if (!texture || !sampler || !output || !command) {
            std::puts("Resource creation failed");
            return 1;
        }
        NSLog(@"Texture usage=%lu (ShaderRead=%lu)", (unsigned long)texture.usage,
              (unsigned long)MTLTextureUsageShaderRead);
        [texture replaceRegion:MTLRegionMake2D(0, 0, 2, 2)
                   mipmapLevel:0
                     withBytes:pixels.data()
                   bytesPerRow:8];
        id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
        [encoder setComputePipelineState:pipeline];
        [encoder setTexture:texture atIndex:0];
        [encoder setSamplerState:sampler atIndex:0];
        [encoder setBuffer:output offset:0 atIndex:0];
        [encoder dispatchThreadgroups:MTLSizeMake(1, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(4, 1, 1)];
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
        if (command.status != MTLCommandBufferStatusCompleted) {
            NSLog(@"Command failed: %@", command.error);
            return 1;
        }
        const auto* values = static_cast<const float*>(output.contents);
        bool equal = true;
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            const auto expected = static_cast<float>(pixels[i]) / 255.0F;
            equal = equal && std::isfinite(values[i]) && values[i] == expected;
            std::printf("%g%c", static_cast<double>(values[i]), i % 4 == 3 ? '\n' : ' ');
        }
        std::puts(equal ? "NATIVE_TEXTURE_EXACT" : "NATIVE_TEXTURE_MISMATCH");
        return equal ? 0 : 1;
    }
}
