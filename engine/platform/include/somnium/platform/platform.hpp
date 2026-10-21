// Owns the SDL session and an optional resizable window without exporting native types.
// The object must be created, polled and destroyed on the window thread.
#pragma once

#include <somnium/core/result.hpp>

#include <cstdint>
#include <memory>
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
};

class Platform final {
  public:
    [[nodiscard]] static Result<std::unique_ptr<Platform>> create(const WindowConfig& config);
    // Initializes the video subsystem for an offscreen device, without a window.
    [[nodiscard]] static Result<std::unique_ptr<Platform>> createHeadless();
    ~Platform();
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    [[nodiscard]] Events pollEvents();
    [[nodiscard]] Result<void> requestQuit();
    // Waits for events only while presentation has no drawable; does not consume the event.
    [[nodiscard]] Result<void> waitEvents() const;

  private:
    friend struct detail::Access;
    friend struct detail::EventAccess;
    struct Impl;
    explicit Platform(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::platform
