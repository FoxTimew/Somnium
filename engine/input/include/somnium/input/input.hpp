// Maps neutral, step-stamped events to the sandbox's fixed action vocabulary.
// Queue storage grows only for event bursts; state and four gamepad slots are allocation-free.
#pragma once

#include <somnium/core/input_event.hpp>
#include <somnium/core/result.hpp>

#include <array>
#include <span>
#include <vector>

namespace somnium::input {
enum class Action : std::uint8_t { Orbit, Reset, Count };
enum class Axis : std::uint8_t {
    MouseYaw,
    MousePitch,
    WheelZoom,
    StickYaw,
    StickPitch,
    TriggerZoom,
    Count
};
struct ButtonState {
    bool pressed = false, released = false, held = false;
};
struct State {
    std::uint64_t step = 0;
    std::array<ButtonState, static_cast<std::size_t>(Action::Count)> actions{};
    std::array<float, static_cast<std::size_t>(Axis::Count)> axes{};
    [[nodiscard]] const ButtonState& action(Action name) const;
    [[nodiscard]] float axis(Axis name) const;
};
class Input final {
  public:
    Input();
    [[nodiscard]] Result<void> enqueue(std::span<const core::InputEvent> events);
    [[nodiscard]] Result<State> advance(std::uint64_t step);

  private:
    struct Pad {
        std::uint32_t id = 0;
        bool connected = false, reset = false;
        std::array<float, 6> axes{};
    };
    void apply(const core::InputEvent& event);
    void updateActions();
    State m_state;
    std::vector<core::InputEvent> m_pending;
    std::array<Pad, 4> m_pads{};
    bool m_keyReset = false, m_orbit = false, m_advanced = false;
};
} // namespace somnium::input
