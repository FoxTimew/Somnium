// Resolves the tool's bindings once, then updates only its private pose at fixed steps.
// Looking uses held-button deltas; neither tool movement nor lens enters scene serialization.
#include "editor_camera.hpp"
#include <algorithm>
#include <cmath>
namespace somnium::debug_ui::detail {
Result<input::Configuration> EditorCamera::configuration() {
    return input::Configuration::parse(R"({"version":1,
      "actions":[{"name":"Look","bindings":[{"source":"mouseButton","control":"Right"}]}],
      "axes":[
        {"name":"X","mode":"rate","bindings":[{"source":"key","control":"D"},{"source":"key","control":"A","invert":true}]},
        {"name":"Y","mode":"rate","bindings":[{"source":"key","control":"E"},{"source":"key","control":"Q","invert":true}]},
        {"name":"Z","mode":"rate","bindings":[{"source":"key","control":"S"},{"source":"key","control":"W","invert":true}]},
        {"name":"LookX","mode":"delta","bindings":[{"source":"mouseX","scale":0.003,"held":"Look"}]},
        {"name":"LookY","mode":"delta","bindings":[{"source":"mouseY","scale":0.003,"held":"Look"}]},
        {"name":"Dolly","mode":"delta","bindings":[{"source":"wheelY"}]}
      ]})");
}
Result<void> EditorCamera::initialize(const scene::World& world,
                                      const input::Configuration& mapping) {
    const auto camera = world.activeCamera;
    if (camera == entt::null)
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Tool camera needs an initial scene camera"});
    m_pose.position = glm::vec3(world.registry().get<Transform>(camera).world[3]);
    m_pose.rotation = glm::quat_cast(glm::mat3(world.registry().get<Transform>(camera).world));
    m_lens = world.registry().get<Camera>(camera);
    const auto forward = m_pose.rotation * glm::vec3(0, 0, -1);
    m_yaw = std::atan2(-forward.x, -forward.z);
    m_pitch = std::asin(std::clamp(forward.y, -1.F, 1.F));
    for (const auto& [name, id] : {std::pair{"X", &m_x},
                                   {"Y", &m_y},
                                   {"Z", &m_z},
                                   {"LookX", &m_lookX},
                                   {"LookY", &m_lookY},
                                   {"Dolly", &m_dolly}}) {
        auto resolved = mapping.axis(name);
        if (!resolved)
            return tl::unexpected(resolved.error());
        *id = *resolved;
    }
    return {};
}
void EditorCamera::update(const input::State& state, double seconds) {
    m_yaw = std::remainder(m_yaw - state.axis(m_lookX), glm::radians(360.F));
    m_pitch = std::clamp(m_pitch - state.axis(m_lookY), glm::radians(-89.F), glm::radians(89.F));
    m_pose.rotation =
        glm::angleAxis(m_yaw, glm::vec3(0, 1, 0)) * glm::angleAxis(m_pitch, glm::vec3(1, 0, 0));
    auto movement = glm::vec3(state.axis(m_x), state.axis(m_y), state.axis(m_z));
    if (glm::length(movement) > 1)
        movement = glm::normalize(movement);
    m_pose.position += m_pose.rotation * (movement * (6.F * static_cast<float>(seconds)) +
                                          glm::vec3(0, 0, -state.axis(m_dolly)));
}
} // namespace somnium::debug_ui::detail
