// Validates fixed-step delivery through an independently compiled game target.
// Lifecycle diagnostics are emitted only at startup and shutdown, not per tick.
#include <somnium/game/foundation_game.hpp>

#include <somnium/core/assert.hpp>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>

#include <array>
#include <cstdio>

namespace somnium::game {
Result<void> FoundationGame::start(runtime::GameContext&) {
    if (m_running) {
        return tl::unexpected(Error{ErrorCode::InvalidState, "Game already started"});
    }
    m_steps = 0;
    m_simulatedSeconds = 0.0;
    m_running = true;
    Log::write("game", Log::Level::Info, "Phase 0 started: window only, no GPU device");
    return {};
}

void FoundationGame::fixedUpdate(const runtime::FixedUpdate& update) {
    SOMNIUM_ASSERT(m_running);
    SOMNIUM_ASSERT(update.tick == m_steps);
    SOMNIUM_PROFILE_SCOPE("game.fixed_update");
    ++m_steps;
    m_simulatedSeconds += update.seconds;
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
