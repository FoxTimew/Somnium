// Implements the sandbox gameplay and owns its orbital camera.
// The existing orbit behavior drives the root camera entity loaded from JSON.
#pragma once

#include <somnium/game/orbit_camera.hpp>
#include <somnium/runtime/gameplay.hpp>

namespace somnium::game {
class FoundationGame final : public runtime::Gameplay {
  public:
    [[nodiscard]] Result<void> start(scene::World& world) override;
    [[nodiscard]] Result<void> fixedUpdate(scene::World& world,
                                           const runtime::FixedUpdate& update) override;
    void stop(scene::World& world) override;

  private:
    std::uint64_t m_steps = 0;
    double m_simulatedSeconds = 0.0;
    bool m_running = false;
    OrbitCamera m_camera;
    entt::entity m_cameraEntity = entt::null;
    bool m_cameraMoving = false;
};
} // namespace somnium::game
