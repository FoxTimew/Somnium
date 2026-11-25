// Owns the sandbox's orbit behavior in gameplay, independently of the render loop.
// A bounded polar angle keeps the view direction away from the world-up singularity.
#pragma once
#include <somnium/runtime/gameplay.hpp>

namespace somnium::game {
class OrbitCamera final {
  public:
    void reset();
    void update(const input::State& input, double seconds);
    [[nodiscard]] renderer::Camera camera() const;

  private:
    float m_yaw = 0.65F, m_pitch = 0.45F, m_distance = 10.F;
};
} // namespace somnium::game
