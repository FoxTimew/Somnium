// Proves orbit bounds, reset and time scaling on the CPU without a GPU device.
// Extreme sustained inputs must still produce a valid perspective camera.
#include <cmath>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <ostream>
#include <somnium/game/foundation_game.hpp>
#include <somnium/game/orbit_camera.hpp>

namespace {
somnium::input::State axes(glm::vec3 values) {
    somnium::input::State state;
    state.axes[static_cast<std::size_t>(somnium::input::Axis::StickYaw)] = values.x;
    state.axes[static_cast<std::size_t>(somnium::input::Axis::StickPitch)] = values.y;
    state.axes[static_cast<std::size_t>(somnium::input::Axis::TriggerZoom)] = values.z;
    return state;
}
} // namespace
TEST_CASE("Gameplay drives the root scene camera from borrowed tick input") {
    somnium::scene::World world;
    auto camera = world.create("camera");
    REQUIRE(camera);
    world.registry().emplace<somnium::Camera>(*camera);
    somnium::game::FoundationGame game;
    REQUIRE(game.start(world));
    const auto before = world.registry().get<somnium::Transform>(*camera).current;
    auto move = axes({1, 0, 0});
    world.beginStep();
    REQUIRE(game.fixedUpdate(world, {1. / 60., 0, move}));
    CHECK(glm::length(world.registry().get<somnium::Transform>(*camera).current.position -
                      before.position) > .01F);
    game.stop(world);
}
TEST_CASE("Orbit analog speed is independent of the number of fixed updates") {
    somnium::game::OrbitCamera a, b;
    const auto move = axes({.4F, -.2F, .1F});
    for (int i = 0; i < 60; ++i)
        a.update(move, 1. / 60.);
    for (int i = 0; i < 120; ++i)
        b.update(move, 1. / 120.);
    CHECK(glm::length(a.pose().position - b.pose().position) < .0001F);
}
TEST_CASE("Pitch and zoom bounds prevent the look-at singularity and camera penetration") {
    somnium::game::OrbitCamera orbit;
    for (float sign : {-1.F, 1.F}) {
        for (int i = 0; i < 2000; ++i)
            orbit.update(axes({1, sign, sign}), 1. / 60.);
        const auto camera = orbit.pose();
        const auto radius = glm::length(camera.position);
        CHECK(radius >= 3.9999F);
        CHECK(radius <= 30.0001F);
        CHECK(std::abs(camera.position.y / radius) < .985F);
        REQUIRE(somnium::scene::validateTrs(camera));
        CHECK(glm::length(camera.rotation * glm::vec3(0, 0, -1) + glm::normalize(camera.position)) <
              .0001F);
    }
    somnium::input::State reset;
    reset.actions[static_cast<std::size_t>(somnium::input::Action::Reset)].pressed = true;
    orbit.update(reset, 1. / 60.);
    const somnium::game::OrbitCamera initial;
    CHECK(glm::length(orbit.pose().position - initial.pose().position) == 0);
}
TEST_CASE("Pointer orbit and wheel are impulses rather than per-second speeds") {
    somnium::game::OrbitCamera a, b;
    somnium::input::State input;
    input.axes[static_cast<std::size_t>(somnium::input::Axis::MouseYaw)] = .5F;
    input.axes[static_cast<std::size_t>(somnium::input::Axis::WheelZoom)] = .2F;
    a.update(input, 1. / 60.);
    b.update(input, 1. / 120.);
    CHECK(glm::length(a.pose().position - b.pose().position) == 0);
    CHECK(glm::length(a.pose().position) == doctest::Approx(10.F * std::exp(-.2F)));
}
