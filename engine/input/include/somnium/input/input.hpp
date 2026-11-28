// Retains events until their exact simulation step, including ordered double taps.
// State views borrow storage until the next advance; configuration is immutable.
#pragma once
#include <array>
#include <memory_resource>
#include <somnium/core/export.hpp>
#include <somnium/core/input_names.hpp>
#include <somnium/input/configuration.hpp>
namespace somnium::input {
struct ButtonState {
    bool pressed = false, released = false, held = false;
};
struct Transition {
    ActionId action;
    bool down = false;
};
struct State {
    std::uint64_t step = 0;
    std::span<const ButtonState> actions;
    std::span<const float> axes;
    std::span<const Transition> transitions;
    [[nodiscard]] SOMNIUM_ENGINE_API const ButtonState& action(ActionId id) const;
    [[nodiscard]] SOMNIUM_ENGINE_API float axis(AxisId id) const;
};
struct Capture {
    bool keyboard = false, mouse = false;
    bool operator==(const Capture&) const = default;
};
class Input final {
  public:
    SOMNIUM_ENGINE_API explicit Input(
        Configuration configuration,
        std::pmr::memory_resource* memory = std::pmr::get_default_resource(),
        std::uint64_t firstStep = 0);
    [[nodiscard]] const Configuration& configuration() const {
        return m_configuration;
    }
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> enqueue(std::span<const core::InputEvent> events);
    // Ordered at the next tick, after already collected events for that tick.
    SOMNIUM_ENGINE_API void setCapture(Capture capture);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<State> advance(std::uint64_t step);
    [[nodiscard]] SOMNIUM_ENGINE_API State state() const;

  private:
    struct Button {
        bool down = false, blocked = false;
    };
    struct Pad {
        std::uint32_t id = 0;
        bool connected = false;
        std::array<Button, core::kGamepadButtonNames.size()> buttons{};
        std::array<float, 6> axes{};
    };
    struct Queued {
        core::InputEvent event;
        std::optional<Capture> capture;
    };
    SOMNIUM_ENGINE_API void apply(const core::InputEvent& event);
    SOMNIUM_ENGINE_API void capture(Capture capture);
    SOMNIUM_ENGINE_API void updateActions();
    [[nodiscard]] SOMNIUM_ENGINE_API float physical(const Binding& binding) const;
    Configuration m_configuration;
    std::pmr::vector<ButtonState> m_actions;
    std::pmr::vector<float> m_axes;
    std::pmr::vector<Transition> m_transitions;
    std::pmr::vector<Queued> m_pending;
    std::array<Button, static_cast<std::size_t>(core::Key::Count)> m_keys{};
    std::array<Button, 6> m_mouse{};
    std::array<Pad, 4> m_pads{};
    Capture m_capture{}, m_requestedCapture{};
    std::uint64_t m_nextStep = 0;
};
} // namespace somnium::input
