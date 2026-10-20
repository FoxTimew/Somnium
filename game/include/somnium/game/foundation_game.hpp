// Implements the phase 0 consumer of the engine's lifecycle contract.
// Only gameplay counters are owned here; engine services remain borrowed.
#pragma once

#include <somnium/runtime/gameplay.hpp>

namespace somnium::game {
class FoundationGame final : public runtime::Gameplay {
  public:
    [[nodiscard]] Result<void> start(runtime::GameContext& context) override;
    void fixedUpdate(const runtime::FixedUpdate& update) override;
    void stop() override;

  private:
    std::uint64_t m_steps = 0;
    double m_simulatedSeconds = 0.0;
    bool m_running = false;
};
} // namespace somnium::game
