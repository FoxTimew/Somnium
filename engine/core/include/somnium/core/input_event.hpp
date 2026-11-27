// Carries raw device input across the platform boundary without native identifiers.
// step is the next simulation tick at collection; motion is pixels, wheel is scroll units.
#pragma once

#include <somnium/core/keys.hpp>

#include <cstdint>

namespace somnium::core {
enum class MouseButton : std::uint8_t { Unknown, Left, Middle, Right, Extra1, Extra2 };
enum class GamepadButton : std::uint8_t {
    Unknown,
    South,
    East,
    West,
    North,
    Back,
    Guide,
    Start,
    LeftStick,
    RightStick,
    LeftShoulder,
    RightShoulder,
    Up,
    Down,
    Left,
    Right
};
enum class GamepadAxis : std::uint8_t { LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger };
enum class InputEventKind : std::uint8_t {
    Key,
    MouseButton,
    MouseMotion,
    MouseWheel,
    GamepadButton,
    GamepadAxis,
    GamepadConnected,
    GamepadDisconnected,
    FocusLost
};
struct InputEvent {
    InputEventKind kind = InputEventKind::FocusLost;
    std::uint64_t step = 0;
    std::uint32_t device = 0;
    Key key = Key::Unknown;
    MouseButton mouseButton = MouseButton::Unknown;
    GamepadButton gamepadButton = GamepadButton::Unknown;
    GamepadAxis gamepadAxis = GamepadAxis::LeftX;
    bool down = false;
    // Relative pointer displacement or wheel units; gamepad raw signed 16-bit value.
    float x = 0, y = 0;
    std::int32_t value = 0;
};
} // namespace somnium::core
