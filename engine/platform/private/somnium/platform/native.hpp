// Private cooperation with the GPU owner and the platform event adapter.
// These declarations are not installed or propagated to module consumers.
#pragma once
#include <SDL3/SDL_video.h>
#include <somnium/platform/platform.hpp>

namespace somnium::platform::detail {
struct Access {
    static SDL_Window* window(Platform& platform);
};
} // namespace somnium::platform::detail
