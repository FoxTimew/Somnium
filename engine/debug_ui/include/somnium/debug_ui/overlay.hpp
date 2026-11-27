// Owns the optional diagnostic overlay and sequences its integration facades.
// Public interfaces contain neither SDL nor ImGui types.
#pragma once
#include <memory>
#include <optional>
#include <somnium/core/result.hpp>
#include <string_view>
namespace somnium {
struct Trs;
struct Camera;
} // namespace somnium
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
#if defined(SOMNIUM_ENABLE_ITERATION)
    [[nodiscard]] static Result<input::Configuration> iterationMapping();
    [[nodiscard]] Result<void> initializeToolCamera(const scene::World& world,
                                                    const input::Configuration& mapping);
    void updateToolCamera(const input::State& state, double seconds);
    [[nodiscard]] const Trs& toolPose() const;
    [[nodiscard]] const Camera& toolLens() const;
    void iterationState(bool playing, std::string_view error);
    [[nodiscard]] std::optional<bool> takePlayRequest();
    void applyEdits(scene::World& world);
#endif
    [[nodiscard]] Result<void> prepare();
    [[nodiscard]] Result<void> render();

  private:
    struct Impl;
    explicit Overlay(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::debug_ui
