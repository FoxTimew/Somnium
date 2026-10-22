// Translates the pinned SDL event vocabulary at the collection boundary.
// Unknown controls remain neutral and repeated keyboard presses are discarded.
#include "input_translation.hpp"
#include <SDL3/SDL_mouse.h>

namespace somnium::platform::detail {
std::optional<core::InputEvent> translateInput(const SDL_Event& event, std::uint64_t step) {
    core::InputEvent result;
    result.step = step;
    using core::InputEventKind;
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (event.key.repeat)
            return {};
        result.kind = InputEventKind::Key;
        result.down = event.key.down;
        switch (event.key.scancode) {
        case SDL_SCANCODE_R:
            result.key = core::Key::R;
            break;
        case SDL_SCANCODE_ESCAPE:
            result.key = core::Key::Escape;
            break;
        case SDL_SCANCODE_SPACE:
            result.key = core::Key::Space;
            break;
        case SDL_SCANCODE_LEFT:
            result.key = core::Key::Left;
            break;
        case SDL_SCANCODE_RIGHT:
            result.key = core::Key::Right;
            break;
        case SDL_SCANCODE_UP:
            result.key = core::Key::Up;
            break;
        case SDL_SCANCODE_DOWN:
            result.key = core::Key::Down;
            break;
        default:
            break;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        result.kind = InputEventKind::MouseButton;
        result.down = event.button.down;
        switch (event.button.button) {
        case SDL_BUTTON_LEFT:
            result.mouseButton = core::MouseButton::Left;
            break;
        case SDL_BUTTON_RIGHT:
            result.mouseButton = core::MouseButton::Right;
            break;
        case SDL_BUTTON_MIDDLE:
            result.mouseButton = core::MouseButton::Middle;
            break;
        case SDL_BUTTON_X1:
            result.mouseButton = core::MouseButton::Extra1;
            break;
        case SDL_BUTTON_X2:
            result.mouseButton = core::MouseButton::Extra2;
            break;
        default:
            break;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        result.kind = InputEventKind::MouseMotion;
        result.x = event.motion.xrel;
        result.y = event.motion.yrel;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        result.kind = InputEventKind::MouseWheel;
        result.x = event.wheel.x;
        result.y = event.wheel.y;
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            result.x = -result.x;
            result.y = -result.y;
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        result.kind = InputEventKind::GamepadButton;
        result.device = event.gbutton.which;
        result.down = event.gbutton.down;
        switch (event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            result.gamepadButton = core::GamepadButton::South;
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
            result.gamepadButton = core::GamepadButton::East;
            break;
        case SDL_GAMEPAD_BUTTON_WEST:
            result.gamepadButton = core::GamepadButton::West;
            break;
        case SDL_GAMEPAD_BUTTON_NORTH:
            result.gamepadButton = core::GamepadButton::North;
            break;
        case SDL_GAMEPAD_BUTTON_BACK:
            result.gamepadButton = core::GamepadButton::Back;
            break;
        case SDL_GAMEPAD_BUTTON_GUIDE:
            result.gamepadButton = core::GamepadButton::Guide;
            break;
        case SDL_GAMEPAD_BUTTON_START:
            result.gamepadButton = core::GamepadButton::Start;
            break;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            result.gamepadButton = core::GamepadButton::LeftStick;
            break;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            result.gamepadButton = core::GamepadButton::RightStick;
            break;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            result.gamepadButton = core::GamepadButton::LeftShoulder;
            break;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            result.gamepadButton = core::GamepadButton::RightShoulder;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            result.gamepadButton = core::GamepadButton::Up;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            result.gamepadButton = core::GamepadButton::Down;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            result.gamepadButton = core::GamepadButton::Left;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            result.gamepadButton = core::GamepadButton::Right;
            break;
        default:
            break;
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        result.kind = InputEventKind::GamepadAxis;
        result.device = event.gaxis.which;
        result.value = event.gaxis.value;
        switch (event.gaxis.axis) {
        case SDL_GAMEPAD_AXIS_LEFTX:
            result.gamepadAxis = core::GamepadAxis::LeftX;
            break;
        case SDL_GAMEPAD_AXIS_LEFTY:
            result.gamepadAxis = core::GamepadAxis::LeftY;
            break;
        case SDL_GAMEPAD_AXIS_RIGHTX:
            result.gamepadAxis = core::GamepadAxis::RightX;
            break;
        case SDL_GAMEPAD_AXIS_RIGHTY:
            result.gamepadAxis = core::GamepadAxis::RightY;
            break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            result.gamepadAxis = core::GamepadAxis::LeftTrigger;
            break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
            result.gamepadAxis = core::GamepadAxis::RightTrigger;
            break;
        default:
            return {};
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
        result.kind = event.type == SDL_EVENT_GAMEPAD_ADDED ? InputEventKind::GamepadConnected
                                                            : InputEventKind::GamepadDisconnected;
        result.device = event.gdevice.which;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        result.kind = InputEventKind::FocusLost;
        break;
    default:
        return {};
    }
    return result;
}
} // namespace somnium::platform::detail
