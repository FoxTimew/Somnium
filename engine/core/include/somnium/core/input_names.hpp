// Shares the neutral vocabulary between configuration and replay readers.
// Device identifiers remain runtime values, never native types.
#pragma once
#include <cmath>
#include <optional>
#include <somnium/core/input_event.hpp>
namespace somnium::core {
inline constexpr std::array kMouseButtonNames{
    std::string_view{"Unknown"}, std::string_view{"Left"},   std::string_view{"Middle"},
    std::string_view{"Right"},   std::string_view{"Extra1"}, std::string_view{"Extra2"}};
inline constexpr std::array kGamepadButtonNames{
    std::string_view{"Unknown"},      std::string_view{"South"},
    std::string_view{"East"},         std::string_view{"West"},
    std::string_view{"North"},        std::string_view{"Back"},
    std::string_view{"Guide"},        std::string_view{"Start"},
    std::string_view{"LeftStick"},    std::string_view{"RightStick"},
    std::string_view{"LeftShoulder"}, std::string_view{"RightShoulder"},
    std::string_view{"Up"},           std::string_view{"Down"},
    std::string_view{"Left"},         std::string_view{"Right"}};
inline constexpr std::array kGamepadAxisNames{
    std::string_view{"LeftX"},  std::string_view{"LeftY"},       std::string_view{"RightX"},
    std::string_view{"RightY"}, std::string_view{"LeftTrigger"}, std::string_view{"RightTrigger"}};
template <typename E, std::size_t N>
[[nodiscard]] std::optional<E> controlFromName(std::string_view name,
                                               const std::array<std::string_view, N>& names) {
    for (std::size_t i = 0; i < N; ++i)
        if (name == names[i])
            return static_cast<E>(i);
    return {};
}
[[nodiscard]] inline bool validInputEvent(const InputEvent& event) {
    return event.kind <= InputEventKind::FocusLost && event.key < Key::Count &&
           event.mouseButton <= MouseButton::Extra2 &&
           event.gamepadButton <= GamepadButton::Right &&
           event.gamepadAxis <= GamepadAxis::RightTrigger && std::isfinite(event.x) &&
           std::isfinite(event.y) && event.value >= -32768 && event.value <= 32767;
}
} // namespace somnium::core
