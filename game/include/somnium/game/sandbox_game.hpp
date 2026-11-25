// Implements the complete reference game through World and compiled input identifiers.
// Rendering content and camera behavior remain outside runtime.
#pragma once
#include <memory_resource>
#include <somnium/game/components.hpp>
namespace somnium::game {
class SandboxGame final : public runtime::Gameplay {
  public:
    [[nodiscard]] Result<void> start(scene::World& world,
                                     const input::Configuration& mapping) override;
    [[nodiscard]] Result<void> fixedUpdate(scene::World& world,
                                           const runtime::FixedUpdate& update) override;
    void stop(scene::World& world) override;

  private:
    [[nodiscard]] Result<void> updateDoors(scene::World& world, const runtime::FixedUpdate& update);
    [[nodiscard]] Result<void> moveCharacter(scene::World& world,
                                             const runtime::FixedUpdate& update);
    [[nodiscard]] Result<void> follow(scene::World& world, const runtime::FixedUpdate& update);
    std::pmr::vector<entt::entity> m_colliders, m_doors;
    // Scratch is reserved once from the loaded scene; no per-tick container allocation.
    std::pmr::vector<CollisionBounds> m_blocks;
    entt::entity m_character = entt::null, m_camera = entt::null;
    OrbitControls m_orbit;
    OrbitState m_initialOrbit;
    input::ActionId m_interact;
    input::AxisId m_moveX, m_moveZ;
    std::uint64_t m_tick = 0;
    bool m_running = false;
};
} // namespace somnium::game
