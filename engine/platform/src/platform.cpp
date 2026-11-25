// Contains SDL initialization, window ownership and native event collection.
// No GPU device, ImGui include or UI-specific conditional belongs here.
#include <somnium/platform/events.hpp>
#include <somnium/platform/native.hpp>
#include <somnium/platform/platform.hpp>

#include "input_translation.hpp"
#include <somnium/core/assert.hpp>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <array>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace somnium::platform {
struct Platform::Impl {
    SDL_Window* window = nullptr;
    bool initialized = false;
    void* observerContext = nullptr;
    void (*observer)(void*, const SDL_Event&) = nullptr;
    std::thread::id owner = std::this_thread::get_id();
    std::vector<core::InputEvent> input;
    std::array<SDL_Gamepad*, 4> gamepads{};

    ~Impl() {
        SOMNIUM_VERIFY(owner == std::this_thread::get_id());
        for (auto* pad : gamepads)
            if (pad)
                SDL_CloseGamepad(pad);
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
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
        return tl::unexpected(Error{ErrorCode::Platform, SDL_GetError()});
    (*session)->m_impl->input.reserve(128);
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

Events Platform::pollEvents(std::uint64_t nextStep) {
    SOMNIUM_VERIFY(m_impl->owner == std::this_thread::get_id());
    SOMNIUM_PROFILE_SCOPE("platform.events");
    Events result;
    m_impl->input.clear();
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (m_impl->observer)
            m_impl->observer(m_impl->observerContext, event);
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
            auto slot = std::ranges::find(m_impl->gamepads, nullptr);
            if (slot == m_impl->gamepads.end()) {
                Log::write("platform", Log::Level::Warning,
                           "Only four simultaneous gamepads are supported");
                continue;
            }
            *slot = SDL_OpenGamepad(event.gdevice.which);
            if (!*slot) {
                Log::write("platform", Log::Level::Warning, SDL_GetError());
                continue;
            }
        } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            for (auto& pad : m_impl->gamepads) {
                if (pad && SDL_GetGamepadID(pad) == event.gdevice.which) {
                    SDL_CloseGamepad(pad);
                    pad = nullptr;
                }
            }
        }
        if (auto input = detail::translateInput(event, nextStep))
            m_impl->input.push_back(*input);
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
    result.input = m_impl->input;
    return result;
}

Result<void> Platform::waitEvents() const {
    SOMNIUM_VERIFY(m_impl->owner == std::this_thread::get_id());
    if (!SDL_WaitEvent(nullptr))
        return tl::unexpected(Error{ErrorCode::Platform, SDL_GetError()});
    return {};
}

SDL_Window* detail::Access::window(Platform& platform) {
    SOMNIUM_VERIFY(platform.m_impl->owner == std::this_thread::get_id());
    return platform.m_impl->window;
}
void detail::EventAccess::setObserver(Platform& platform, void* context,
                                      void (*observer)(void*, const SDL_Event&)) {
    SOMNIUM_VERIFY(platform.m_impl->owner == std::this_thread::get_id());
    platform.m_impl->observerContext = context;
    platform.m_impl->observer = observer;
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
