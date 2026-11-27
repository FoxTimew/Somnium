// Drives the tool camera without a window and proves that it never mutates World.
// Its independently resolved mapping consumes deltas and respects bounded pitch.
#include "editor_camera.hpp"
#include <doctest/doctest.h>
#include <somnium/scene/serialization.hpp>
namespace somnium::debug_ui::detail {
TEST_CASE("Tool camera has separate bindings and cannot enter a World snapshot") {
    auto mapping = EditorCamera::configuration();
    REQUIRE(mapping);
    CHECK_FALSE(mapping->action("Interact"));
    scene::World world;
    auto camera = world.create("camera");
    REQUIRE(camera);
    world.registry().emplace<Camera>(*camera);
    world.activeCamera = *camera;
    REQUIRE(world.propagate());
    scene::CodecRegistry codecs;
    REQUIRE(scene::registerBuiltinCodecs(codecs));
    auto saved = scene::snapshot(world, codecs);
    REQUIRE(saved);
    EditorCamera tool;
    REQUIRE(tool.initialize(world, *mapping));
    std::vector<input::ButtonState> actions(mapping->actions().size());
    std::vector<float> axes(mapping->axes().size());
    axes[mapping->axis("X")->value] = 1;
    for (std::uint64_t tick = 0; tick < 60; ++tick)
        tool.update({tick, actions, axes, {}}, 1. / 60.);
    CHECK(tool.pose().position.x == doctest::Approx(6));
    axes[mapping->axis("LookY")->value] = 10000;
    tool.update({60, actions, axes, {}}, 1. / 60.);
    CHECK(scene::validateTrs(tool.pose()));
    const auto forward = tool.pose().rotation * glm::vec3(0, 0, -1);
    CHECK(forward.y > -1);
    CHECK(tool.lens().verticalFov == world.registry().get<Camera>(*camera).verticalFov);
    const auto after = scene::snapshot(world, codecs);
    REQUIRE(after);
    CHECK(after->canonical == saved->canonical);
    CHECK(after->temporal == saved->temporal);
}
TEST_CASE("Tool camera can edit a scene without a gameplay camera") {
    auto mapping = EditorCamera::configuration();
    REQUIRE(mapping);
    scene::World world;
    scene::CodecRegistry codecs;
    REQUIRE(scene::registerBuiltinCodecs(codecs));
    const auto before = scene::snapshot(world, codecs);
    REQUIRE(before);
    EditorCamera tool;
    REQUIRE(tool.initialize(world, *mapping));
    CHECK(scene::validateTrs(tool.pose()));
    CHECK(tool.pose().position == glm::vec3(0, 3, 6));
    const auto after = scene::snapshot(world, codecs);
    REQUIRE(after);
    CHECK(after->canonical == before->canonical);
    CHECK(after->temporal == before->temporal);
    CHECK(world.activeCamera == entt::null);
}
} // namespace somnium::debug_ui::detail
