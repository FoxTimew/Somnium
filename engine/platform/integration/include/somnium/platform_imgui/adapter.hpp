// SDL-free facade of the optional official platform backend.
// Initialization and frame sequencing belong to debug_ui.
#pragma once
#include <memory>
#include <somnium/core/result.hpp>
namespace somnium::platform {
class Platform;
}
namespace somnium::platform_imgui {
class Adapter final {
  public:
    static Result<std::unique_ptr<Adapter>> create(platform::Platform& platform);
    ~Adapter();
    Adapter(const Adapter&) = delete;
    Adapter& operator=(const Adapter&) = delete;
    void newFrame();

  private:
    explicit Adapter(platform::Platform& platform) : m_platform(platform) {}
    platform::Platform& m_platform;
};
} // namespace somnium::platform_imgui
