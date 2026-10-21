// Owns the optional diagnostic overlay and sequences its integration facades.
// Public interfaces contain neither SDL nor ImGui types.
#pragma once
#include <memory>
#include <somnium/core/result.hpp>
namespace somnium::platform {
class Platform;
}
namespace somnium::gpu {
class Device;
}
namespace somnium::debug_ui {
class Overlay final {
  public:
    static Result<std::unique_ptr<Overlay>> create(platform::Platform& platform,
                                                   gpu::Device& device);
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    [[nodiscard]] Result<void> prepare(double frameSeconds);
    [[nodiscard]] Result<void> render();

  private:
    struct Impl;
    explicit Overlay(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::debug_ui
