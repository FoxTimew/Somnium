// Private native borrows for integration targets; never installed or exported.
// The device owns recording and presentation, including error-path submission.
#pragma once
#include <SDL3/SDL_gpu.h>
#include <somnium/gpu/device.hpp>
namespace somnium::gpu::detail {
struct NativeFrame {
    SDL_GPUCommandBuffer* commands;
    SDL_GPUTexture* color;
    FrameSize extent;
};
struct Activity {
    std::uint64_t draws = 0;
    std::uint64_t uploads = 0;
    std::uint64_t submissions = 0;
};
struct Access {
    static SDL_GPUDevice* device(Device& owner);
    static SDL_GPUTextureFormat presentationFormat(Device& owner);
    static Result<NativeFrame> frame(Device& owner);
    static void count(Device& owner, Activity activity);
};
} // namespace somnium::gpu::detail
