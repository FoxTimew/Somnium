// Proves orbit bounds, reset and time scaling on the CPU without a GPU device.
// Extreme sustained inputs must still produce a valid perspective camera.
#include <cmath>
#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <somnium/game/foundation_game.hpp>
#include <somnium/game/orbit_camera.hpp>
#include <somnium/renderer/renderer.hpp>

namespace {
class NoRead final : public somnium::core::ByteReader {
  public:
    somnium::Result<somnium::core::Bytes> read(std::string_view) const override {
        FAIL("Gameplay must request a model without loading it");
        return tl::unexpected(somnium::Error{somnium::ErrorCode::Io, "Unexpected I/O"});
    }
};
somnium::input::State axes(glm::vec3 values) {
    somnium::input::State state;
    state.axes[static_cast<std::size_t>(somnium::input::Axis::StickYaw)] = values.x;
    state.axes[static_cast<std::size_t>(somnium::input::Axis::StickPitch)] = values.y;
    state.axes[static_cast<std::size_t>(somnium::input::Axis::TriggerZoom)] = values.z;
    return state;
}
} // namespace
TEST_CASE("Gameplay selects a logical model and consumes the borrowed tick input") {
    NoRead reader;
    somnium::runtime::GameContext context(reader);
    CHECK_FALSE(context.requestModel("../escape.glb"));
    somnium::game::FoundationGame game;
    REQUIRE(game.start(context));
    CHECK(context.modelPath() == "models/orbit-cube.glb");
    const auto before = game.camera();
    auto move = axes({1, 0, 0});
    game.fixedUpdate({1. / 60., 0, move});
    CHECK(glm::length(game.camera().eye - before.eye) > .01F);
    game.stop();
}
TEST_CASE("Orbit analog speed is independent of the number of fixed updates") {
    somnium::game::OrbitCamera a, b;
    const auto move = axes({.4F, -.2F, .1F});
    for (int i = 0; i < 60; ++i)
        a.update(move, 1. / 60.);
    for (int i = 0; i < 120; ++i)
        b.update(move, 1. / 120.);
    CHECK(glm::length(a.camera().eye - b.camera().eye) < .0001F);
}
TEST_CASE("Pitch and zoom bounds prevent the look-at singularity and camera penetration") {
    somnium::game::OrbitCamera orbit;
    for (float sign : {-1.F, 1.F}) {
        for (int i = 0; i < 2000; ++i)
            orbit.update(axes({1, sign, sign}), 1. / 60.);
        const auto camera = orbit.camera();
        const auto radius = glm::length(camera.eye);
        CHECK(radius >= 3.9999F);
        CHECK(radius <= 30.0001F);
        CHECK(std::abs(camera.eye.y / radius) < .985F);
        REQUIRE(somnium::renderer::cameraFrame(camera, {1280, 720}));
    }
    somnium::input::State reset;
    reset.actions[static_cast<std::size_t>(somnium::input::Action::Reset)].pressed = true;
    orbit.update(reset, 1. / 60.);
    const somnium::game::OrbitCamera initial;
    CHECK(glm::length(orbit.camera().eye - initial.camera().eye) == 0);
}
TEST_CASE("Pointer orbit and wheel are impulses rather than per-second speeds") {
    somnium::game::OrbitCamera a, b;
    somnium::input::State input;
    input.axes[static_cast<std::size_t>(somnium::input::Axis::MouseYaw)] = .5F;
    input.axes[static_cast<std::size_t>(somnium::input::Axis::WheelZoom)] = .2F;
    a.update(input, 1. / 60.);
    b.update(input, 1. / 120.);
    CHECK(glm::length(a.camera().eye - b.camera().eye) == 0);
    CHECK(glm::length(a.camera().eye) == doctest::Approx(10.F * std::exp(-.2F)));
}
