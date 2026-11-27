// Proves orbit bounds, reset and time scaling on the CPU without a GPU device.
// Extreme sustained inputs must still produce a valid perspective camera.
#include <cmath>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <ostream>
#include <somnium/game/orbit_camera.hpp>

namespace {
constexpr somnium::game::OrbitControls controls{{0}, {0}, {1}, {2}, {3}, {4}, {5}};
struct Synthetic {
    std::array<somnium::input::ButtonState, 1> actions{};
    std::array<float, 6> values{};
    somnium::input::State state() const {
        return {0, actions, values, {}};
    }
};
Synthetic axes(glm::vec3 values) {
    Synthetic result;
    result.values[3] = values.x;
    result.values[4] = values.y;
    result.values[5] = values.z;
    return result;
}
} // namespace
TEST_CASE("Orbit analog speed is independent of the number of fixed updates") {
    somnium::game::OrbitCamera a, b;
    const auto move = axes({.4F, -.2F, .1F});
    for (int i = 0; i < 60; ++i)
        a.update(move.state(), controls, 1. / 60.);
    for (int i = 0; i < 120; ++i)
        b.update(move.state(), controls, 1. / 120.);
    CHECK(glm::length(a.pose().position - b.pose().position) < .0001F);
}
TEST_CASE("Pitch and zoom bounds prevent the look-at singularity and camera penetration") {
    somnium::game::OrbitCamera orbit;
    for (float sign : {-1.F, 1.F}) {
        for (int i = 0; i < 2000; ++i)
            orbit.update(axes({1, sign, sign}).state(), controls, 1. / 60.);
        const auto camera = orbit.pose();
        const auto radius = glm::length(camera.position);
        CHECK(radius >= 3.9999F);
        CHECK(radius <= 30.0001F);
        CHECK(std::abs(camera.position.y / radius) < .985F);
        REQUIRE(somnium::scene::validateTrs(camera));
        CHECK(glm::length(camera.rotation * glm::vec3(0, 0, -1) + glm::normalize(camera.position)) <
              .0001F);
    }
    Synthetic reset;
    reset.actions[0].pressed = true;
    orbit.update(reset.state(), controls, 1. / 60.);
    const somnium::game::OrbitCamera initial;
    CHECK(glm::length(orbit.pose().position - initial.pose().position) == 0);
}
TEST_CASE("Pointer orbit and wheel are impulses rather than per-second speeds") {
    somnium::game::OrbitCamera a, b;
    Synthetic input;
    input.values[0] = .5F;
    input.values[2] = .2F;
    a.update(input.state(), controls, 1. / 60.);
    b.update(input.state(), controls, 1. / 120.);
    CHECK(glm::length(a.pose().position - b.pose().position) == 0);
    CHECK(glm::length(a.pose().position) == doctest::Approx(10.F * std::exp(-.2F)));
}
