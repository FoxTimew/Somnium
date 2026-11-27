// Proves configuration errors, lossless step delivery and capture policy on the CPU.
// Uses game-independent control names and synthetic neutral device events.
#include <doctest/doctest.h>
#include <limits>
#include <somnium/input/input.hpp>
using namespace somnium;
using core::InputEvent;
using core::InputEventKind;
namespace {
constexpr auto mapping = R"({"version":1,"actions":[
{"name":"drag","bindings":[{"source":"mouseButton","control":"Right"}]},
{"name":"fire","bindings":[{"source":"key","control":"F24"},{"source":"gamepadButton","control":"South"}]}],
"axes":[{"name":"look","mode":"delta","bindings":[{"source":"mouseX","held":"drag","scale":0.01}]},
{"name":"zoom","mode":"delta","bindings":[{"source":"wheelY","scale":0.2}]},
{"name":"move","mode":"rate","bindings":[{"source":"key","control":"A","scale":-1},{"source":"key","control":"D"},{"source":"gamepadAxis","control":"LeftX","deadzone":0.15}]},
{"name":"trigger","mode":"rate","bindings":[{"source":"gamepadAxis","control":"RightTrigger"},{"source":"gamepadAxis","control":"LeftTrigger","invert":true}]}]})";
input::Configuration configuration() {
    auto c = input::Configuration::parse(mapping);
    REQUIRE(c);
    return *c;
}
} // namespace
TEST_CASE("A newly activated tool starts at the current tick without replaying prior events") {
    auto config = configuration();
    const auto fire = *config.action("fire");
    input::Input input(config, std::pmr::get_default_resource(), 40);
    CHECK_FALSE(input.advance(0));
    const std::array old{
        InputEvent{.kind = InputEventKind::Key, .step = 39, .key = core::Key::F24, .down = true}};
    CHECK_FALSE(input.enqueue(old));
    auto idle = input.advance(40);
    REQUIRE(idle);
    CHECK(idle->transitions.empty());
    CHECK_FALSE(idle->action(fire).held);
    const std::array fresh{
        InputEvent{.kind = InputEventKind::Key, .step = 41, .key = core::Key::F24, .down = true}};
    REQUIRE(input.enqueue(fresh));
    auto pressed = input.advance(41);
    REQUIRE(pressed);
    CHECK(pressed->action(fire).pressed);
    auto held = input.advance(42);
    REQUIRE(held);
    CHECK(held->action(fire).held);
    CHECK_FALSE(held->action(fire).pressed);
}
TEST_CASE("Consecutive ticks preserve future events and an ordered double tap") {
    auto config = configuration();
    const auto fire = *config.action("fire");
    input::Input input(config);
    const std::array events{
        InputEvent{.kind = InputEventKind::Key, .step = 2, .key = core::Key::F24, .down = true},
        InputEvent{.kind = InputEventKind::Key, .step = 2, .key = core::Key::F24, .down = false},
        InputEvent{.kind = InputEventKind::Key, .step = 2, .key = core::Key::F24, .down = true},
        InputEvent{.kind = InputEventKind::Key, .step = 2, .key = core::Key::F24, .down = false}};
    REQUIRE(input.enqueue(events));
    REQUIRE(input.advance(0));
    CHECK_FALSE(input.advance(2));
    REQUIRE(input.advance(1));
    auto state = input.advance(2);
    REQUIRE(state);
    CHECK(state->action(fire).pressed);
    CHECK(state->action(fire).released);
    CHECK_FALSE(state->action(fire).held);
    REQUIRE(state->transitions.size() == 4);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(state->transitions[i].action == fire);
        CHECK(state->transitions[i].down == (i % 2 == 0));
    }
    state = input.advance(3);
    REQUIRE(state);
    CHECK(state->transitions.empty());
    CHECK_FALSE(input.enqueue(events));
}
TEST_CASE("Conditional deltas follow event order, are unbounded and consumed exactly once") {
    auto config = configuration();
    input::Input input(config);
    const std::array events{InputEvent{.kind = InputEventKind::MouseMotion, .x = 900},
                            InputEvent{.kind = InputEventKind::MouseButton,
                                       .mouseButton = core::MouseButton::Right,
                                       .down = true},
                            InputEvent{.kind = InputEventKind::MouseMotion, .x = 250},
                            InputEvent{.kind = InputEventKind::MouseButton,
                                       .mouseButton = core::MouseButton::Right,
                                       .down = false},
                            InputEvent{.kind = InputEventKind::MouseMotion, .x = 900},
                            InputEvent{.kind = InputEventKind::MouseWheel, .y = 20}};
    REQUIRE(input.enqueue(events));
    auto state = input.advance(0);
    REQUIRE(state);
    CHECK(state->axis(*config.axis("look")) == doctest::Approx(2.5));
    CHECK(state->axis(*config.axis("zoom")) == 4);
    state = input.advance(1);
    REQUIRE(state);
    CHECK(state->axis(*config.axis("look")) == 0);
    CHECK(state->axis(*config.axis("zoom")) == 0);
}
TEST_CASE("Capture releases once and requires a fresh press after return") {
    auto config = configuration();
    input::Input input(config);
    auto fire = *config.action("fire");
    InputEvent event{.kind = InputEventKind::Key, .key = core::Key::F24, .down = true};
    REQUIRE(input.enqueue(std::span(&event, 1)));
    REQUIRE(input.advance(0));
    input.setCapture({true, true});
    auto state = input.advance(1);
    REQUIRE(state);
    CHECK(state->action(fire).released);
    input.setCapture({false, false});
    state = input.advance(2);
    REQUIRE(state);
    CHECK_FALSE(state->action(fire).held);
    CHECK(state->transitions.empty());
    event.step = 3;
    REQUIRE(input.enqueue(std::span(&event, 1)));
    state = input.advance(3);
    REQUIRE(state);
    CHECK_FALSE(state->action(fire).held);
    event.step = 4;
    event.down = false;
    REQUIRE(input.enqueue(std::span(&event, 1)));
    REQUIRE(input.advance(4));
    event.step = 5;
    event.down = true;
    REQUIRE(input.enqueue(std::span(&event, 1)));
    state = input.advance(5);
    REQUIRE(state);
    CHECK(state->action(fire).pressed);
    input.setCapture({true, true});
    input.setCapture({true, true});
    state = input.advance(6);
    REQUIRE(state);
    REQUIRE(state->transitions.size() == 1);
    event.step = 7;
    event.down = false;
    REQUIRE(input.enqueue(std::span(&event, 1)));
    REQUIRE(input.advance(7));
    event.step = 8;
    event.down = true;
    REQUIRE(input.enqueue(std::span(&event, 1)));
    REQUIRE(input.advance(8));
    input.setCapture({});
    state = input.advance(9);
    REQUIRE(state);
    CHECK_FALSE(state->action(fire).held);
}
TEST_CASE("Gamepad normalization, deadzone and OR sources survive disconnect") {
    auto config = configuration();
    input::Input input(config);
    auto fire = *config.action("fire");
    const std::array events{
        InputEvent{.kind = InputEventKind::GamepadConnected, .device = 7},
        InputEvent{.kind = InputEventKind::GamepadAxis,
                   .device = 7,
                   .gamepadAxis = core::GamepadAxis::LeftX,
                   .value = -32768},
        InputEvent{.kind = InputEventKind::GamepadAxis,
                   .device = 7,
                   .gamepadAxis = core::GamepadAxis::RightTrigger,
                   .value = 32767},
        InputEvent{.kind = InputEventKind::GamepadButton,
                   .device = 7,
                   .gamepadButton = core::GamepadButton::South,
                   .down = true},
        InputEvent{.kind = InputEventKind::Key, .key = core::Key::F24, .down = true}};
    REQUIRE(input.enqueue(events));
    auto state = input.advance(0);
    REQUIRE(state);
    CHECK(state->axis(*config.axis("move")) == -1);
    CHECK(state->axis(*config.axis("trigger")) == 1);
    CHECK(state->action(fire).pressed);
    InputEvent small{.kind = InputEventKind::GamepadAxis,
                     .step = 1,
                     .device = 7,
                     .gamepadAxis = core::GamepadAxis::LeftX,
                     .value = 4000};
    REQUIRE(input.enqueue(std::span(&small, 1)));
    state = input.advance(1);
    REQUIRE(state);
    CHECK(state->axis(*config.axis("move")) == 0);
    InputEvent removed{.kind = InputEventKind::GamepadDisconnected, .step = 2, .device = 7};
    REQUIRE(input.enqueue(std::span(&removed, 1)));
    state = input.advance(2);
    REQUIRE(state);
    CHECK(state->axis(*config.axis("trigger")) == 0);
    CHECK(state->action(fire).held);
    CHECK_FALSE(state->action(fire).released);
    InputEvent lost{.kind = InputEventKind::FocusLost, .step = 3};
    REQUIRE(input.enqueue(std::span(&lost, 1)));
    state = input.advance(3);
    REQUIRE(state);
    CHECK(state->action(fire).released);
}
TEST_CASE("Malformed mapping and physical conflicts fail before replacing configuration") {
    const std::array invalid{
        R"({"version":2,"actions":[],"axes":[]})",
        R"({"version":1,"actions":[],"axes":[],"extra":0})",
        R"({"version":1,"actions":[{"name":"a","bindings":[{"source":"key","control":"NoKey"}]}],"axes":[]})",
        R"({"version":1,"actions":[{"name":"a","bindings":[{"source":"gamepadButton","control":"South"}]},{"name":"b","bindings":[{"source":"gamepadButton","control":"South","slot":0}]}],"axes":[]})",
        R"({"version":1,"actions":[],"axes":[{"name":"a","mode":"rate","bindings":[{"source":"mouseX"}]}]})",
        R"({"version":1,"actions":[],"axes":[{"name":"a","mode":"delta","bindings":[{"source":"mouseX","held":"missing"}]}]})",
        R"({"version":1,"actions":[],"axes":[{"name":"a","mode":"rate","bindings":[{"source":"gamepadAxis","control":"LeftX","deadzone":1}]}]})",
        R"({"version":1,"actions":[{"name":"a","bindings":[{"source":"key","control":"A"},{"source":"key","control":"A"}]}],"axes":[]})"};
    auto config = configuration();
    for (auto text : invalid) {
        auto result = input::Configuration::parse(text);
        CHECK_FALSE(result);
        if (!result)
            CHECK_FALSE(result.error().message.empty());
    }
    CHECK(config.action("fire"));
    CHECK_FALSE(config.axis("fire"));
    input::Input input(config);
    const std::array events{InputEvent{.kind = InputEventKind::MouseWheel, .y = 1},
                            InputEvent{.kind = InputEventKind::MouseWheel,
                                       .y = std::numeric_limits<float>::infinity()}};
    CHECK_FALSE(input.enqueue(events));
    auto state = input.advance(0);
    REQUIRE(state);
    CHECK(state->axis(*config.axis("zoom")) == 0);
}

TEST_CASE(
    "Distinct gamepad slots may bind separately, any overlaps and triggers keep their range") {
    auto config = input::Configuration::parse(R"({"version":1,"actions":[],"axes":[
        {"name":"one","mode":"rate","bindings":[{"source":"gamepadAxis","control":"LeftX","slot":0,"deadzone":0.2}]},
        {"name":"two","mode":"rate","bindings":[{"source":"gamepadAxis","control":"LeftX","slot":1,"invert":true}]}]})");
    REQUIRE(config);
    input::Input input(*config);
    const std::array events{InputEvent{.kind = InputEventKind::GamepadConnected, .device = 11},
                            InputEvent{.kind = InputEventKind::GamepadConnected, .device = 42},
                            InputEvent{.kind = InputEventKind::GamepadAxis,
                                       .device = 11,
                                       .gamepadAxis = core::GamepadAxis::LeftX,
                                       .value = 19660},
                            InputEvent{.kind = InputEventKind::GamepadAxis,
                                       .device = 42,
                                       .gamepadAxis = core::GamepadAxis::LeftX,
                                       .value = 32767}};
    REQUIRE(input.enqueue(events));
    auto state = input.advance(0);
    REQUIRE(state);
    CHECK(state->axis(*config->axis("one")) == doctest::Approx(.5).epsilon(.0001));
    CHECK(state->axis(*config->axis("two")) == -1);
}
TEST_CASE("Mouse capture consumes releases but suppresses captured deltas and held presses") {
    auto config = configuration();
    input::Input input(config);
    const auto drag = *config.action("drag"), fire = *config.action("fire");
    const std::array events{
        InputEvent{.kind = InputEventKind::MouseButton,
                   .mouseButton = core::MouseButton::Right,
                   .down = true},
        InputEvent{.kind = InputEventKind::Key, .key = core::Key::F24, .down = true}};
    REQUIRE(input.enqueue(events));
    REQUIRE(input.advance(0));
    input.setCapture({false, true});
    const InputEvent delta{.kind = InputEventKind::MouseMotion, .step = 1, .x = 500};
    REQUIRE(input.enqueue(std::span(&delta, 1)));
    auto state = input.advance(1);
    REQUIRE(state);
    CHECK(state->action(drag).released);
    CHECK(state->action(fire).held);
    CHECK(state->axis(*config.axis("look")) == 0);
    const InputEvent release{
        .kind = InputEventKind::MouseButton, .step = 2, .mouseButton = core::MouseButton::Right};
    REQUIRE(input.enqueue(std::span(&release, 1)));
    REQUIRE(input.advance(2));
    input.setCapture({});
    const std::array resumed{InputEvent{.kind = InputEventKind::MouseButton,
                                        .step = 3,
                                        .mouseButton = core::MouseButton::Right,
                                        .down = true},
                             InputEvent{.kind = InputEventKind::MouseMotion, .step = 3, .x = 500}};
    REQUIRE(input.enqueue(resumed));
    state = input.advance(3);
    REQUIRE(state);
    CHECK(state->action(drag).pressed);
    CHECK(state->axis(*config.axis("look")) == 5);
}

namespace {
class CountingMemory final : public std::pmr::memory_resource {
  public:
    unsigned allocations = 0;

  private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        ++allocations;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }
    void do_deallocate(void* memory, std::size_t bytes, std::size_t alignment) override {
        std::pmr::new_delete_resource()->deallocate(memory, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};
} // namespace
TEST_CASE("Reserved PMR storage does not allocate during ordinary fixed steps") {
    CountingMemory memory;
    input::Input input(configuration(), &memory);
    const auto allocations = memory.allocations;
    for (std::uint64_t step = 0; step < 120; ++step) {
        const std::array events{
            InputEvent{
                .kind = InputEventKind::Key, .step = step, .key = core::Key::F24, .down = true},
            InputEvent{
                .kind = InputEventKind::Key, .step = step, .key = core::Key::F24, .down = false}};
        REQUIRE(input.enqueue(events));
        REQUIRE(input.advance(step));
    }
    CHECK(memory.allocations == allocations);
}
