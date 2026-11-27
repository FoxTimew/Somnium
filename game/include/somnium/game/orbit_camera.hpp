// Owns the sandbox's orbit behavior in gameplay, independently of the render loop.
// A bounded polar angle keeps the view direction away from the world-up singularity.
#pragma once
#include <somnium/runtime/gameplay.hpp>

namespace somnium::game {
struct OrbitControls {
    input::ActionId reset;
    input::AxisId mouseYaw, mousePitch, wheelZoom, stickYaw, stickPitch, triggerZoom;
    [[nodiscard]] static Result<OrbitControls> resolve(const input::Configuration& mapping);
};
struct OrbitState {
    float yaw = .65F, pitch = .45F, distance = 10.F;
};
class OrbitCamera final {
  public:
    explicit OrbitCamera(OrbitState state = {}) : m_state(state) {}
    [[nodiscard]] OrbitState state() const {
        return m_state;
    }
    void reset();
    void update(const input::State& input, const OrbitControls& controls, double seconds);
    [[nodiscard]] Trs pose(glm::vec3 target = glm::vec3(0)) const;

  private:
    OrbitState m_state;
};
} // namespace somnium::game
