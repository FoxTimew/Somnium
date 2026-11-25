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
namespace somnium::scene {
class World;
}
namespace somnium::input {
class Configuration;
struct State;
struct Capture;
} // namespace somnium::input
namespace somnium::debug_ui {
class Overlay final {
  public:
    static Result<std::unique_ptr<Overlay>> create(platform::Platform& platform,
                                                   gpu::Device& device);
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    [[nodiscard]] input::Capture update(double frameSeconds, const scene::World& world,
                                        const input::Configuration& mapping,
                                        const input::State& inputs, bool replay);
    void applyEdits(scene::World& world);
    [[nodiscard]] Result<void> prepare();
    [[nodiscard]] Result<void> render();

  private:
    struct Impl;
    explicit Overlay(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::debug_ui
