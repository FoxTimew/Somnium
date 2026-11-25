// Selects the sandbox model and applies fixed-step input to its gameplay camera.
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
void logCamera(const renderer::Camera& camera) {
    const auto eye = camera.eye;
    Log::write("game", Log::Level::Info, "Orbit pose: eye=({:.4f},{:.4f},{:.4f}) distance={:.4f}",
               eye.x, eye.y, eye.z, glm::length(eye));
}
} // namespace
Result<void> FoundationGame::start(runtime::GameContext& context) {
    if (m_running) {
        return tl::unexpected(Error{ErrorCode::InvalidState, "Game already started"});
    }
    auto requested = context.requestModel("models/orbit-cube.glb");
    if (!requested)
        return requested;
    m_camera.reset();
    logCamera(m_camera.camera());
    m_cameraMoving = false;
    context.lightDirection = {0, 0, 1};
    context.ambient = .15F;
    context.clearColor = {.025F, .035F, .065F, 1};
    m_steps = 0;
    m_simulatedSeconds = 0.0;
    m_running = true;
    Log::write("game", Log::Level::Info, "Gameplay session started");
    return {};
}

void FoundationGame::fixedUpdate(const runtime::FixedUpdate& update) {
    SOMNIUM_ASSERT(m_running);
    SOMNIUM_ASSERT(update.tick == m_steps);
    SOMNIUM_ASSERT(update.inputs.step == update.tick);
    SOMNIUM_PROFILE_SCOPE("game.fixed_update");
    ++m_steps;
    m_simulatedSeconds += update.seconds;
    const auto before = m_camera.camera().eye;
    m_camera.update(update.inputs, update.seconds);
    const auto eye = m_camera.camera().eye;
    if (before != eye)
        m_cameraMoving = true;
    else if (m_cameraMoving) {
        logCamera(m_camera.camera());
        m_cameraMoving = false;
    }
}

void FoundationGame::stop() {
    SOMNIUM_ASSERT(m_running);
    m_running = false;
    std::array<char, 160> message{};
    std::snprintf(message.data(), message.size(), "Stopped after %llu fixed steps (%.3f s)",
                  static_cast<unsigned long long>(m_steps), m_simulatedSeconds);
    Log::write("game", Log::Level::Info, message.data());
}
} // namespace somnium::game
