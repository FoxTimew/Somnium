// Exercises tick delivery, device mixing and normalization without a window or SDL.
// Press/release pulses and delayed simulation must survive frame-rate differences.
#include <doctest/doctest.h>
#include <limits>
#include <somnium/input/input.hpp>

using namespace somnium;
using core::InputEvent;
using core::InputEventKind;
using input::Action;
using input::Axis;

TEST_CASE("Events wait for their tick and a same-tick tap keeps both edges") {
    input::Input input;
    const std::array events{
        InputEvent{.kind = InputEventKind::Key, .step = 2, .key = core::Key::R, .down = true},
        InputEvent{.kind = InputEventKind::Key, .step = 2, .key = core::Key::R, .down = false}};
    REQUIRE(input.enqueue(events));
    auto first = input.advance(0);
    REQUIRE(first);
    CHECK_FALSE(first->action(Action::Reset).pressed);
    auto tap = input.advance(2);
    REQUIRE(tap);
    CHECK(tap->action(Action::Reset).pressed);
    CHECK(tap->action(Action::Reset).released);
    CHECK_FALSE(tap->action(Action::Reset).held);
    auto next = input.advance(3);
    REQUIRE(next);
    CHECK_FALSE(next->action(Action::Reset).pressed);
    CHECK_FALSE(next->action(Action::Reset).released);
    CHECK_FALSE(input.advance(3));
    CHECK_FALSE(input.enqueue(events));
}
TEST_CASE("Held orbit gates pointer impulses and wheel is consumed exactly once") {
    input::Input input;
    const std::array events{InputEvent{.kind = InputEventKind::MouseMotion, .x = 99},
                            InputEvent{.kind = InputEventKind::MouseButton,
                                       .mouseButton = core::MouseButton::Right,
                                       .down = true},
                            InputEvent{.kind = InputEventKind::MouseMotion, .x = 25, .y = -50},
                            InputEvent{.kind = InputEventKind::MouseWheel, .y = 2}};
    REQUIRE(input.enqueue(events));
    auto first = input.advance(0);
    REQUIRE(first);
    CHECK(first->action(Action::Orbit).pressed);
    CHECK(first->axis(Axis::MouseYaw) == doctest::Approx(.25));
    CHECK(first->axis(Axis::MousePitch) == doctest::Approx(-.5));
    CHECK(first->axis(Axis::WheelZoom) == doctest::Approx(.4));
    auto second = input.advance(1);
    REQUIRE(second);
    CHECK(second->action(Action::Orbit).held);
    CHECK_FALSE(second->action(Action::Orbit).pressed);
    CHECK(second->axis(Axis::MouseYaw) == 0);
    CHECK(second->axis(Axis::WheelZoom) == 0);
    const InputEvent lost{.kind = InputEventKind::FocusLost, .step = 2};
    REQUIRE(input.enqueue(std::span(&lost, 1)));
    auto focus = input.advance(2);
    REQUIRE(focus);
    CHECK(focus->action(Action::Orbit).released);
    CHECK_FALSE(focus->action(Action::Orbit).held);
}
TEST_CASE("Pad ranges, deadzone, triggers and disconnect preserve other held sources") {
    input::Input input;
    const std::array events{
        InputEvent{.kind = InputEventKind::GamepadConnected, .device = 7},
        InputEvent{.kind = InputEventKind::GamepadAxis,
                   .device = 7,
                   .gamepadAxis = core::GamepadAxis::RightX,
                   .value = -32768},
        InputEvent{.kind = InputEventKind::GamepadAxis,
                   .device = 7,
                   .gamepadAxis = core::GamepadAxis::RightY,
                   .value = 4000},
        InputEvent{.kind = InputEventKind::GamepadAxis,
                   .device = 7,
                   .gamepadAxis = core::GamepadAxis::RightTrigger,
                   .value = 32767},
        InputEvent{.kind = InputEventKind::GamepadButton,
                   .device = 7,
                   .gamepadButton = core::GamepadButton::South,
                   .down = true},
        InputEvent{.kind = InputEventKind::Key, .key = core::Key::R, .down = true}};
    REQUIRE(input.enqueue(events));
    auto first = input.advance(0);
    REQUIRE(first);
    CHECK(first->axis(Axis::StickYaw) == -1);
    CHECK(first->axis(Axis::StickPitch) == 0);
    CHECK(first->axis(Axis::TriggerZoom) == 1);
    CHECK(first->action(Action::Reset).pressed);
    auto next = input.advance(1);
    REQUIRE(next);
    CHECK(next->axis(Axis::StickYaw) == -1);
    const InputEvent removed{.kind = InputEventKind::GamepadDisconnected, .step = 2, .device = 7};
    REQUIRE(input.enqueue(std::span(&removed, 1)));
    auto disconnected = input.advance(2);
    REQUIRE(disconnected);
    CHECK(disconnected->axis(Axis::StickYaw) == 0);
    CHECK(disconnected->axis(Axis::TriggerZoom) == 0);
    CHECK(disconnected->action(Action::Reset).held);
    CHECK_FALSE(disconnected->action(Action::Reset).released);
    const InputEvent release{.kind = InputEventKind::Key, .step = 3, .key = core::Key::R};
    REQUIRE(input.enqueue(std::span(&release, 1)));
    auto released = input.advance(3);
    REQUIRE(released);
    CHECK(released->action(Action::Reset).released);
}
TEST_CASE("Invalid batches are rejected atomically and axes are bounded") {
    input::Input input;
    const std::array invalid{InputEvent{.kind = InputEventKind::MouseWheel, .y = 5},
                             InputEvent{.kind = InputEventKind::MouseMotion,
                                        .x = std::numeric_limits<float>::quiet_NaN()}};
    CHECK_FALSE(input.enqueue(invalid));
    auto empty = input.advance(0);
    REQUIRE(empty);
    CHECK(empty->axis(Axis::WheelZoom) == 0);
    const InputEvent scroll{.kind = InputEventKind::MouseWheel, .step = 1, .y = 99};
    REQUIRE(input.enqueue(std::span(&scroll, 1)));
    auto state = input.advance(1);
    REQUIRE(state);
    CHECK(state->axis(Axis::WheelZoom) == 1);
}
