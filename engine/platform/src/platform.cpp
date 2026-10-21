// Contains SDL initialization, window ownership and native event collection.
// No GPU device, ImGui include or UI-specific conditional belongs here.
#include <somnium/platform/platform.hpp>

#include <somnium/core/assert.hpp>
#include <somnium/core/profile.hpp>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <string>
#include <thread>
#include <utility>

namespace somnium::platform {
struct Platform::Impl {
    SDL_Window* window = nullptr;
    bool initialized = false;
    std::thread::id owner = std::this_thread::get_id();

    ~Impl() {
        SOMNIUM_VERIFY(owner == std::this_thread::get_id());
        if (window) {
            SDL_DestroyWindow(window);
        }
        if (initialized) {
            SDL_Quit();
        }
    }
};

Platform::Platform(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {
    SOMNIUM_PROFILE_ALLOC(this, sizeof(Platform));
}

Platform::~Platform() {
    SOMNIUM_PROFILE_FREE(this);
}

Result<std::unique_ptr<Platform>> Platform::create(const WindowConfig& config) {
    if (config.width <= 0 || config.height <= 0 ||
        config.title.find('\0') != std::string_view::npos) {
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid window configuration"});
    }
    auto session = createHeadless();
    if (!session) {
        return tl::unexpected(session.error());
    }
    const std::string title(config.title);
    (*session)->m_impl->window =
        SDL_CreateWindow(title.c_str(), config.width, config.height, SDL_WINDOW_RESIZABLE);
    if (!(*session)->m_impl->window) {
        return tl::unexpected(Error{ErrorCode::Platform, SDL_GetError()});
    }
    return session;
}

Result<std::unique_ptr<Platform>> Platform::createHeadless() {
    if (SDL_WasInit(0) != 0) {
        return tl::unexpected(Error{ErrorCode::InvalidState, "SDL session already owned"});
    }
    auto impl = std::make_unique<Impl>();
    SDL_SetMainReady();
    impl->initialized = true; // SDL_Quit also cleans up a partially failed initialization.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return tl::unexpected(Error{ErrorCode::Platform, SDL_GetError()});
    }
    return std::unique_ptr<Platform>(new Platform(std::move(impl)));
}

Events Platform::pollEvents() {
    SOMNIUM_VERIFY(m_impl->owner == std::this_thread::get_id());
    SOMNIUM_PROFILE_SCOPE("platform.events");
    Events result;
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            result.quitRequested = true;
        } else if (m_impl->window && event.type >= SDL_EVENT_WINDOW_FIRST &&
                   event.type <= SDL_EVENT_WINDOW_LAST &&
                   event.window.windowID == SDL_GetWindowID(m_impl->window)) {
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                result.quitRequested = true;
            }
            if (event.type == SDL_EVENT_WINDOW_RESTORED ||
                event.type == SDL_EVENT_WINDOW_MINIMIZED) {
                result.clockResetRequested = true;
            }
        }
    }
    return result;
}

void Platform::wait(std::uint32_t milliseconds) const {
    SDL_Delay(milliseconds);
}

Result<void> Platform::requestQuit() {
    SDL_Event event{};
    event.type = SDL_EVENT_QUIT;
    if (!SDL_PushEvent(&event)) {
        return tl::unexpected(Error{ErrorCode::Platform, SDL_GetError()});
    }
    return {};
}
} // namespace somnium::platform
