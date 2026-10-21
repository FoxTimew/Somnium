// Keeps the native event stream inside platform and its event adapter.
// The observer borrows each event only for the duration of collection.
#pragma once
#include <SDL3/SDL_events.h>
#include <somnium/platform/platform.hpp>
namespace somnium::platform::detail {
struct EventAccess {
    static void setObserver(Platform& platform, void* context,
                            void (*observer)(void*, const SDL_Event&));
};
} // namespace somnium::platform::detail
