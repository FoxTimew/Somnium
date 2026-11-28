// Owns the SDL session and an optional resizable window without exporting native types.
// The object must be created, polled and destroyed on the window thread.
#pragma once
#include <somnium/core/export.hpp>

#include <somnium/core/input_event.hpp>
#include <somnium/core/result.hpp>
#include <somnium/platform/input_replay.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace somnium::platform {
namespace detail {
struct Access;
struct EventAccess;
} // namespace detail
struct WindowConfig {
    std::string_view title = "Somnium";
    int width = 1280;
    int height = 720;
};

struct Events {
    bool quitRequested = false;
    bool clockResetRequested = false;
    // Borrowed until the next poll; copy into input before polling again.
    std::span<const core::InputEvent> input;
};

class Platform final {
  public:
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<std::unique_ptr<Platform>>
    create(const WindowConfig& config);
    // Initializes the video subsystem for an offscreen device, without a window.
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<std::unique_ptr<Platform>> createHeadless();
    SOMNIUM_ENGINE_API ~Platform();
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    [[nodiscard]] SOMNIUM_ENGINE_API Events pollEvents(std::uint64_t nextStep = 0);
    SOMNIUM_ENGINE_API void setReplay(InputReplay replay);
    [[nodiscard]] SOMNIUM_ENGINE_API bool replaying() const;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<std::span<const core::InputEvent>>
    replayEvents(std::uint64_t step);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> requestQuit();
    // Waits for events only while presentation has no drawable; does not consume the event.
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> waitEvents() const;

  private:
    friend struct detail::Access;
    friend struct detail::EventAccess;
    struct Impl;
    SOMNIUM_ENGINE_API explicit Platform(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::platform
