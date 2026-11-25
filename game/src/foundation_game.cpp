// Applies fixed-step input to the root camera entity selected by the sandbox scene.
// Lifecycle diagnostics are emitted only at startup and shutdown, not per tick.
#include <somnium/game/foundation_game.hpp>

#include <somnium/core/assert.hpp>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>

#include <array>
#include <cstdio>
#include <glm/geometric.hpp>

namespace somnium::game {
namespace {
void logCamera(const Trs& camera) {
    const auto eye = camera.position;
    Log::write("game", Log::Level::Info, "Orbit pose: eye=({:.4f},{:.4f},{:.4f}) distance={:.4f}",
               eye.x, eye.y, eye.z, glm::length(eye));
}
} // namespace
Result<void> FoundationGame::start(scene::World& world, const input::Configuration& mapping) {
    if (m_running) {
        return tl::unexpected(Error{ErrorCode::InvalidState, "Game already started"});
    }
    auto controls = OrbitControls::resolve(mapping);
    if (!controls)
        return tl::unexpected(controls.error());
    m_controls = *controls;
    auto camera = world.find("camera");
    if (!camera)
        return tl::unexpected(camera.error());
    if (!world.registry().all_of<Camera>(*camera) ||
        world.registry().get<Hierarchy>(*camera).parent != entt::null)
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Sandbox camera must be a root Camera"});
    m_cameraEntity = *camera;
    m_camera.reset();
    logCamera(m_camera.pose());
    m_cameraMoving = false;
    auto posed = world.teleport(m_cameraEntity, m_camera.pose());
    if (!posed)
        return posed;
    m_steps = 0;
    m_simulatedSeconds = 0.0;
    m_running = true;
    Log::write("game", Log::Level::Info, "Gameplay session started");
    return {};
}

Result<void> FoundationGame::fixedUpdate(scene::World& world, const runtime::FixedUpdate& update) {
    SOMNIUM_ASSERT(m_running);
    SOMNIUM_ASSERT(update.tick == m_steps);
    SOMNIUM_ASSERT(update.inputs.step == update.tick);
    SOMNIUM_PROFILE_SCOPE("game.fixed_update");
    ++m_steps;
    m_simulatedSeconds += update.seconds;
    const auto before = m_camera.pose().position;
    m_camera.update(update.inputs, m_controls, update.seconds);
    const auto eye = m_camera.pose().position;
    if (before != eye)
        m_cameraMoving = true;
    else if (m_cameraMoving) {
        logCamera(m_camera.pose());
        m_cameraMoving = false;
    }
    return update.inputs.action(m_controls.reset).pressed
               ? world.teleport(m_cameraEntity, m_camera.pose())
               : world.setLocal(m_cameraEntity, m_camera.pose());
}

void FoundationGame::stop(scene::World&) {
    SOMNIUM_ASSERT(m_running);
    m_running = false;
    std::array<char, 160> message{};
    std::snprintf(message.data(), message.size(), "Stopped after %llu fixed steps (%.3f s)",
                  static_cast<unsigned long long>(m_steps), m_simulatedSeconds);
    Log::write("game", Log::Level::Info, message.data());
}
} // namespace somnium::game
