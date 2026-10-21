// Private cooperation with the GPU owner and the platform event adapter.
// These declarations are not installed or propagated to module consumers.
#pragma once
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <somnium/platform/platform.hpp>

namespace somnium::platform::detail {
struct Access {
    static SDL_Window* window(Platform& platform);
    static void setEventObserver(Platform& platform, void* context,
                                 void (*observer)(void*, const SDL_Event&));
};
} // namespace somnium::platform::detail
