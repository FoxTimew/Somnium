// Combines one-shot pointer impulses with time-scaled analog motion.
// Spherical reconstruction avoids accumulating a drifting Euler rotation matrix.
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <numbers>
#include <somnium/game/orbit_camera.hpp>

namespace somnium::game {
Result<OrbitControls> OrbitControls::resolve(const input::Configuration& mapping) {
    OrbitControls result;
    auto reset = mapping.action("Reset");
    if (!reset)
        return tl::unexpected(reset.error());
    result.reset = *reset;
    auto mouseYaw = mapping.axis("MouseYaw");
    if (!mouseYaw)
        return tl::unexpected(mouseYaw.error());
    result.mouseYaw = *mouseYaw;
    auto mousePitch = mapping.axis("MousePitch");
    if (!mousePitch)
        return tl::unexpected(mousePitch.error());
    result.mousePitch = *mousePitch;
    auto wheelZoom = mapping.axis("WheelZoom");
    if (!wheelZoom)
        return tl::unexpected(wheelZoom.error());
    result.wheelZoom = *wheelZoom;
    auto stickYaw = mapping.axis("StickYaw");
    if (!stickYaw)
        return tl::unexpected(stickYaw.error());
    result.stickYaw = *stickYaw;
    auto stickPitch = mapping.axis("StickPitch");
    if (!stickPitch)
        return tl::unexpected(stickPitch.error());
    result.stickPitch = *stickPitch;
    auto triggerZoom = mapping.axis("TriggerZoom");
    if (!triggerZoom)
        return tl::unexpected(triggerZoom.error());
    result.triggerZoom = *triggerZoom;
    return result;
}
void OrbitCamera::reset() {
    m_yaw = .65F;
    m_pitch = .45F;
    m_distance = 10.F;
}
void OrbitCamera::update(const input::State& input, const OrbitControls& controls, double seconds) {
    if (input.action(controls.reset).pressed) {
        reset();
        return;
    }
    const auto delta = static_cast<float>(seconds);
    m_yaw += .5F * input.axis(controls.mouseYaw) + 1.5F * delta * input.axis(controls.stickYaw);
    m_yaw = std::remainder(m_yaw, 2.F * std::numbers::pi_v<float>);
    m_pitch -=
        .5F * input.axis(controls.mousePitch) + 1.5F * delta * input.axis(controls.stickPitch);
    constexpr float limit = 80.F * std::numbers::pi_v<float> / 180.F;
    m_pitch = std::clamp(m_pitch, -limit, limit);
    const auto zoom = input.axis(controls.wheelZoom) + delta * input.axis(controls.triggerZoom);
    m_distance = std::clamp(m_distance * std::exp(-zoom), 4.F, 30.F);
}
Trs OrbitCamera::pose() const {
    const auto horizontal = m_distance * std::cos(m_pitch);
    const glm::vec3 eye{horizontal * std::sin(m_yaw), m_distance * std::sin(m_pitch),
                        horizontal * std::cos(m_yaw)};
    return {eye, glm::quatLookAtRH(glm::normalize(-eye), glm::vec3(0, 1, 0)), glm::vec3(1)};
}
} // namespace somnium::game
