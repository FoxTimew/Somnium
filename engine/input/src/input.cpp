// Applies ordered physical events and capture boundaries to compiled bindings.
// Delta axes are consumed once; rates persist and aggregate into [-1,1].
#include <algorithm>
#include <cmath>
#include <somnium/core/assert.hpp>
#include <somnium/core/log.hpp>
#include <somnium/input/input.hpp>
#include <utility>
namespace somnium::input {
namespace {
template <typename T, typename A>
void reserveBurst(std::vector<T, A>& values, std::size_t required) {
    if (required <= values.capacity())
        return;
    Log::write("input", Log::Level::Warning, "Input burst: growing reserved storage to {}",
               required);
    values.reserve(std::max(required, values.capacity() * 2));
}
float bounded(double value) {
    constexpr auto limit = static_cast<double>(std::numeric_limits<float>::max());
    return static_cast<float>(std::clamp(value, -limit, limit));
}
} // namespace
const ButtonState& State::action(ActionId id) const {
    SOMNIUM_ASSERT(id.value < actions.size());
    return actions[id.value];
}
float State::axis(AxisId id) const {
    SOMNIUM_ASSERT(id.value < axes.size());
    return axes[id.value];
}
Input::Input(Configuration configuration, std::pmr::memory_resource* memory)
    : m_configuration(std::move(configuration)),
      m_actions(m_configuration.actions().size(), memory),
      m_axes(m_configuration.axes().size(), memory), m_transitions(memory), m_pending(memory) {
    m_pending.reserve(128);
    m_transitions.reserve(128);
}
State Input::state() const {
    return {m_nextStep == 0 ? 0 : m_nextStep - 1, m_actions, m_axes, m_transitions};
}
Result<void> Input::enqueue(std::span<const core::InputEvent> events) {
    auto previous =
        m_pending.empty() ? m_nextStep : std::max(m_nextStep, m_pending.back().event.step);
    for (const auto& event : events) {
        if (event.step < previous || !core::validInputEvent(event))
            return tl::unexpected(
                Error{ErrorCode::InvalidArgument, "Invalid or out-of-order input event"});
        previous = event.step;
    }
    reserveBurst(m_pending, m_pending.size() + events.size());
    for (const auto& event : events)
        m_pending.push_back({event, {}});
    return {};
}
void Input::setCapture(Capture value) {
    if (value == m_requestedCapture)
        return;
    m_requestedCapture = value;
    reserveBurst(m_pending, m_pending.size() + 1);
    const auto where = std::ranges::upper_bound(m_pending, m_nextStep, {},
                                                [](const Queued& q) { return q.event.step; });
    m_pending.insert(where, Queued{core::InputEvent{.step = m_nextStep}, value});
}
void Input::capture(Capture value) {
    if (value.keyboard && !m_capture.keyboard)
        for (auto& button : m_keys)
            button.blocked |= button.down;
    if (value.mouse && !m_capture.mouse)
        for (auto& button : m_mouse)
            button.blocked |= button.down;
    m_capture = value;
    updateActions();
}
float Input::physical(const Binding& binding) const {
    const auto& source = binding.source;
    auto held = [](const Button& button) { return button.down && !button.blocked ? 1.F : 0.F; };
    if (source.kind == SourceKind::Key)
        return m_capture.keyboard ? 0.F : held(m_keys[source.control]);
    if (source.kind == SourceKind::MouseButton)
        return m_capture.mouse ? 0.F : held(m_mouse[source.control]);
    float value = 0;
    for (std::size_t i = 0; i < m_pads.size(); ++i) {
        const auto& pad = m_pads[i];
        if (!pad.connected || (source.slot != -1 && source.slot != static_cast<int>(i)))
            continue;
        if (source.kind == SourceKind::GamepadButton)
            value += held(pad.buttons[source.control]);
        else if (source.kind == SourceKind::GamepadAxis) {
            const auto raw = pad.axes[source.control];
            value += std::abs(raw) <= binding.deadzone
                         ? 0.F
                         : std::copysign(
                               (std::abs(raw) - binding.deadzone) / (1.F - binding.deadzone), raw);
        }
    }
    return value;
}
void Input::updateActions() {
    for (std::size_t i = 0; i < m_actions.size(); ++i) {
        const bool held =
            std::ranges::any_of(m_configuration.actions()[i].bindings,
                                [&](const Binding& binding) { return physical(binding) != 0; });
        auto& button = m_actions[i];
        if (held != button.held) {
            reserveBurst(m_transitions, m_transitions.size() + 1);
            m_transitions.push_back({ActionId{static_cast<std::uint32_t>(i)}, held});
            button.pressed |= held;
            button.released |= !held;
            button.held = held;
        }
    }
}
void Input::apply(const core::InputEvent& event) {
    using core::InputEventKind;
    auto press = [](Button& button, bool down, bool captured) {
        if (!down)
            button = {};
        else if (!button.down)
            button = {true, captured};
    };
    if (event.kind == InputEventKind::FocusLost) {
        m_keys.fill({});
        m_mouse.fill({});
        for (auto& pad : m_pads) {
            pad.buttons.fill({});
            pad.axes.fill(0);
        }
        // Previously accumulated impulses happened before focus loss and remain delivered.
    } else if (event.kind == InputEventKind::Key) {
        press(m_keys[static_cast<std::size_t>(event.key)], event.down, m_capture.keyboard);
    } else if (event.kind == InputEventKind::MouseButton) {
        press(m_mouse[static_cast<std::size_t>(event.mouseButton)], event.down, m_capture.mouse);
    } else if ((event.kind == InputEventKind::MouseMotion ||
                event.kind == InputEventKind::MouseWheel) &&
               !m_capture.mouse) {
        const bool motion = event.kind == InputEventKind::MouseMotion;
        for (std::size_t i = 0; i < m_axes.size(); ++i) {
            const auto& axis = m_configuration.axes()[i];
            if (axis.mode != AxisMode::Delta)
                continue;
            for (const auto& binding : axis.bindings) {
                if (binding.held && !m_actions[binding.held->value].held)
                    continue;
                const auto kind = binding.source.kind;
                if (motion != (kind == SourceKind::MouseX || kind == SourceKind::MouseY))
                    continue;
                const auto value =
                    kind == SourceKind::MouseX || kind == SourceKind::WheelX ? event.x : event.y;
                m_axes[i] = bounded(static_cast<double>(m_axes[i]) +
                                    static_cast<double>(value) * binding.scale);
            }
        }
    } else if (event.kind >= InputEventKind::GamepadButton &&
               event.kind <= InputEventKind::GamepadDisconnected) {
        auto pad = std::ranges::find_if(
            m_pads, [&](const Pad& value) { return value.connected && value.id == event.device; });
        if (event.kind == InputEventKind::GamepadConnected && pad == m_pads.end()) {
            pad = std::ranges::find_if(m_pads, [](const Pad& value) { return !value.connected; });
            if (pad != m_pads.end()) {
                *pad = {};
                pad->id = event.device;
                pad->connected = true;
            } else
                Log::write("input", Log::Level::Warning,
                           "Only four simultaneous gamepads are supported");
        }
        if (pad != m_pads.end()) {
            if (event.kind == InputEventKind::GamepadDisconnected)
                *pad = {};
            else if (event.kind == InputEventKind::GamepadButton)
                press(pad->buttons[static_cast<std::size_t>(event.gamepadButton)], event.down,
                      false);
            else if (event.kind == InputEventKind::GamepadAxis) {
                const auto value = static_cast<float>(event.value);
                pad->axes[static_cast<std::size_t>(event.gamepadAxis)] =
                    event.gamepadAxis >= core::GamepadAxis::LeftTrigger
                        ? std::max(value / 32767.F, 0.F)
                        : value / (value < 0 ? 32768.F : 32767.F);
            }
        }
    }
    updateActions();
}
Result<State> Input::advance(std::uint64_t step) {
    if (step != m_nextStep || step == std::numeric_limits<std::uint64_t>::max())
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Input ticks must be consecutive, starting at zero"});
    std::ranges::fill(m_axes, 0.F);
    m_transitions.clear();
    for (auto& button : m_actions)
        button.pressed = button.released = false;
    auto end = m_pending.begin();
    while (end != m_pending.end() && end->event.step == step) {
        if (end->capture)
            capture(*end->capture);
        else
            apply(end->event);
        ++end;
    }
    m_pending.erase(m_pending.begin(), end);
    for (std::size_t i = 0; i < m_axes.size(); ++i) {
        const auto& axis = m_configuration.axes()[i];
        if (axis.mode != AxisMode::Rate)
            continue;
        double value = 0;
        for (const auto& binding : axis.bindings)
            value += static_cast<double>(physical(binding)) * binding.scale;
        m_axes[i] = static_cast<float>(std::clamp(value, -1., 1.));
    }
    ++m_nextStep;
    return state();
}
} // namespace somnium::input
