// Combines one-shot pointer impulses with time-scaled analog motion.
// Spherical reconstruction avoids accumulating a drifting Euler rotation matrix.
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <numbers>
#include <somnium/game/orbit_camera.hpp>

namespace somnium::game {
void OrbitCamera::reset() {
    m_yaw = .65F;
    m_pitch = .45F;
    m_distance = 10.F;
}
void OrbitCamera::update(const input::State& input, double seconds) {
    if (input.action(input::Action::Reset).pressed) {
        reset();
        return;
    }
    const auto delta = static_cast<float>(seconds);
    m_yaw +=
        .5F * input.axis(input::Axis::MouseYaw) + 1.5F * delta * input.axis(input::Axis::StickYaw);
    m_yaw = std::remainder(m_yaw, 2.F * std::numbers::pi_v<float>);
    m_pitch -= .5F * input.axis(input::Axis::MousePitch) +
               1.5F * delta * input.axis(input::Axis::StickPitch);
    constexpr float limit = 80.F * std::numbers::pi_v<float> / 180.F;
    m_pitch = std::clamp(m_pitch, -limit, limit);
    const auto zoom =
        input.axis(input::Axis::WheelZoom) + delta * input.axis(input::Axis::TriggerZoom);
    m_distance = std::clamp(m_distance * std::exp(-zoom), 4.F, 30.F);
}
Trs OrbitCamera::pose() const {
    const auto horizontal = m_distance * std::cos(m_pitch);
    const glm::vec3 eye{horizontal * std::sin(m_yaw), m_distance * std::sin(m_pitch),
                        horizontal * std::cos(m_yaw)};
    return {eye, glm::quatLookAtRH(glm::normalize(-eye), glm::vec3(0, 1, 0)), glm::vec3(1)};
}
} // namespace somnium::game
