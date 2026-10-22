// Retains transitions until their simulation tick and normalizes the raw device ranges.
// Pointer impulses apply once; sticks and held buttons persist until release or focus loss.
#include <somnium/core/assert.hpp>
#include <somnium/input/input.hpp>

#include <algorithm>
#include <cmath>

namespace somnium::input {
namespace {
float stick(std::int32_t value) {
    const float normalized =
        std::clamp(static_cast<float>(value) / (value < 0 ? 32768.F : 32767.F), -1.F, 1.F);
    constexpr float deadzone = 0.15F;
    return std::abs(normalized) <= deadzone
               ? 0.F
               : std::copysign((std::abs(normalized) - deadzone) / (1.F - deadzone), normalized);
}
} // namespace
const ButtonState& State::action(Action name) const {
    SOMNIUM_ASSERT(name < Action::Count);
    return actions[static_cast<std::size_t>(name)];
}
float State::axis(Axis name) const {
    SOMNIUM_ASSERT(name < Axis::Count);
    return axes[static_cast<std::size_t>(name)];
}
Input::Input() {
    m_pending.reserve(128);
}
Result<void> Input::enqueue(std::span<const core::InputEvent> events) {
    auto previous = m_pending.empty() ? (m_advanced ? m_state.step + 1 : 0) : m_pending.back().step;
    for (const auto& event : events) {
        if (event.step < previous || !std::isfinite(event.x) || !std::isfinite(event.y) ||
            event.kind > core::InputEventKind::FocusLost ||
            event.gamepadAxis > core::GamepadAxis::RightTrigger || event.value < -32768 ||
            event.value > 32767)
            return tl::unexpected(
                Error{ErrorCode::InvalidArgument, "Invalid or out-of-order input event"});
        previous = event.step;
    }
    m_pending.insert(m_pending.end(), events.begin(), events.end());
    return {};
}
void Input::updateActions() {
    const bool reset = m_keyReset || std::ranges::any_of(m_pads, [](const Pad& pad) {
                           return pad.connected && pad.reset;
                       });
    const std::array held{m_orbit, reset};
    for (std::size_t i = 0; i < held.size(); ++i) {
        auto& button = m_state.actions[i];
        button.pressed |= held[i] && !button.held;
        button.released |= !held[i] && button.held;
        button.held = held[i];
    }
}
void Input::apply(const core::InputEvent& event) {
    using core::InputEventKind;
    if (event.kind == InputEventKind::FocusLost) {
        m_keyReset = m_orbit = false;
        for (auto& pad : m_pads) {
            pad.reset = false;
            pad.axes.fill(0);
        }
        m_state.axes.fill(0);
    } else if (event.kind == InputEventKind::Key && event.key == core::Key::R) {
        m_keyReset = event.down;
    } else if (event.kind == InputEventKind::MouseButton &&
               event.mouseButton == core::MouseButton::Right) {
        m_orbit = event.down;
    } else if (event.kind == InputEventKind::MouseMotion && m_orbit) {
        m_state.axes[static_cast<std::size_t>(Axis::MouseYaw)] += event.x / 100.F;
        m_state.axes[static_cast<std::size_t>(Axis::MousePitch)] += event.y / 100.F;
    } else if (event.kind == InputEventKind::MouseWheel) {
        m_state.axes[static_cast<std::size_t>(Axis::WheelZoom)] += event.y / 5.F;
    } else if (event.kind >= InputEventKind::GamepadButton &&
               event.kind <= InputEventKind::GamepadDisconnected) {
        auto found = std::ranges::find_if(
            m_pads, [&](const Pad& pad) { return pad.connected && pad.id == event.device; });
        if (event.kind == InputEventKind::GamepadConnected && found == m_pads.end()) {
            found = std::ranges::find_if(m_pads, [](const Pad& pad) { return !pad.connected; });
            if (found != m_pads.end())
                *found = Pad{event.device, true, false, {}};
        }
        if (found != m_pads.end()) {
            if (event.kind == InputEventKind::GamepadDisconnected)
                *found = {};
            else if (event.kind == InputEventKind::GamepadButton &&
                     event.gamepadButton == core::GamepadButton::South)
                found->reset = event.down;
            else if (event.kind == InputEventKind::GamepadAxis) {
                found->axes[static_cast<std::size_t>(event.gamepadAxis)] =
                    event.gamepadAxis >= core::GamepadAxis::LeftTrigger
                        ? std::clamp(static_cast<float>(event.value) / 32767.F, 0.F, 1.F)
                        : stick(event.value);
            }
        }
    }
    updateActions();
}
Result<State> Input::advance(std::uint64_t step) {
    if (m_advanced && step <= m_state.step)
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Input ticks must increase"});
    m_advanced = true;
    m_state.step = step;
    m_state.axes.fill(0);
    for (auto& button : m_state.actions)
        button.pressed = button.released = false;
    auto end = m_pending.begin();
    while (end != m_pending.end() && end->step <= step)
        apply(*end++);
    m_pending.erase(m_pending.begin(), end);
    for (const auto& pad : m_pads) {
        if (!pad.connected)
            continue;
        m_state.axes[static_cast<std::size_t>(Axis::StickYaw)] += pad.axes[2];
        m_state.axes[static_cast<std::size_t>(Axis::StickPitch)] += pad.axes[3];
        m_state.axes[static_cast<std::size_t>(Axis::TriggerZoom)] += pad.axes[5] - pad.axes[4];
    }
    for (auto& value : m_state.axes)
        value = std::clamp(value, -1.F, 1.F);
    return m_state;
}
} // namespace somnium::input
