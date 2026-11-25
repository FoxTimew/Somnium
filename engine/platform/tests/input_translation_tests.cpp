// Checks native translation in platform; the input test binary never sees SDL.
// All cases use event values directly and require no initialized device.
#include "input_translation.hpp"
#include <SDL3/SDL_mouse.h>
#include <doctest/doctest.h>

TEST_CASE("Native controls become raw Somnium events stamped with the next step") {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_R;
    event.key.down = true;
    auto key = somnium::platform::detail::translateInput(event, 42);
    REQUIRE(key);
    if (!key)
        return;
    CHECK(key->step == 42);
    CHECK(key->key == somnium::core::Key::R);
    CHECK(key->down);
    event.key.repeat = true;
    CHECK_FALSE(somnium::platform::detail::translateInput(event, 42));
    event = {};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.y = 1.25F;
    event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    auto wheel = somnium::platform::detail::translateInput(event, 43);
    REQUIRE(wheel);
    if (!wheel)
        return;
    CHECK(wheel->y == -1.25F);
    event = {};
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.which = 99;
    event.gaxis.axis = SDL_GAMEPAD_AXIS_RIGHTX;
    event.gaxis.value = -32768;
    auto pad = somnium::platform::detail::translateInput(event, 44);
    REQUIRE(pad);
    if (!pad)
        return;
    CHECK(pad->device == 99);
    CHECK(pad->gamepadAxis == somnium::core::GamepadAxis::RightX);
    CHECK(pad->value == -32768);
}
