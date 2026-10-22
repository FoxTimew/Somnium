// Implements the sandbox gameplay and owns its orbital camera.
// Model selection is a logical request; loading and rendering remain engine services.
#pragma once

#include <somnium/game/orbit_camera.hpp>
#include <somnium/runtime/gameplay.hpp>

namespace somnium::game {
class FoundationGame final : public runtime::Gameplay {
  public:
    [[nodiscard]] Result<void> start(runtime::GameContext& context) override;
    void fixedUpdate(const runtime::FixedUpdate& update) override;
    [[nodiscard]] renderer::Camera camera() const override {
        return m_camera.camera();
    }
    void stop() override;

  private:
    std::uint64_t m_steps = 0;
    double m_simulatedSeconds = 0.0;
    bool m_running = false;
    OrbitCamera m_camera;
};
} // namespace somnium::game
