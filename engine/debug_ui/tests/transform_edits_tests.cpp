// Proves editor requests are inert until the runtime's fixed-step boundary.
// World rejects invalid edits and synchronizes teleport history for interpolation.
#include "transform_edits.hpp"
#include <doctest/doctest.h>
#include <limits>
#include <somnium/scene/rotation.hpp>
namespace somnium::debug_ui::detail {
TEST_CASE("Inspector coalesces edits and applies one teleport only at the next step") {
    scene::World world;
    const auto parent = world.create("parent"), child = world.create("child");
    REQUIRE(parent);
    REQUIRE(child);
    REQUIRE(world.reparent(*child, *parent));
    auto childPose = world.registry().get<Transform>(*child).current;
    childPose.position = {1, 0, 0};
    REQUIRE(world.teleport(*child, childPose));
    TransformEdits edits;
    Trs local;
    local.position = {2, 0, 0};
    edits.queue("parent", local);
    local.position = {4, 0, 0};
    local.rotation = scene::rotationDegrees({0, 90, 0});
    edits.queue("parent", local);
    CHECK(world.registry().get<Transform>(*parent).current.position.x == 0);
    world.beginStep();
    REQUIRE(edits.apply(world));
    REQUIRE(world.propagate());
    CHECK(edits.empty());
    const auto& result = world.registry().get<Transform>(*parent);
    CHECK(result.current.position.x == 4);
    CHECK(result.previous.position == result.current.position);
    CHECK(glm::length(glm::vec3(world.registry().get<Transform>(*child).world[3]) -
                      glm::vec3(4, 0, -1)) < .00001F);
    auto next = result.current;
    next.position.x = 8;
    REQUIRE(world.setLocal(*parent, next));
    REQUIRE(edits.apply(world));
    CHECK(world.registry().get<Transform>(*parent).current.position.x == 8);
}
TEST_CASE(
    "Inspector reports invalid camera scale and stale selections without replaying requests") {
    scene::World world;
    auto camera = world.create("camera");
    REQUIRE(camera);
    world.registry().emplace<Camera>(*camera);
    TransformEdits edits;
    Trs pose;
    pose.scale = glm::vec3(2);
    edits.queue("camera", pose);
    world.beginStep();
    auto rejected = edits.apply(world);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().message.find("camera") != std::string::npos);
    CHECK(world.registry().get<Transform>(*camera).current.scale == glm::vec3(1));
    CHECK(edits.empty());
    pose = {};
    edits.queue("camera", pose);
    REQUIRE(world.destroy(*camera));
    CHECK_FALSE(edits.apply(world));
    CHECK(edits.empty());
    REQUIRE(edits.apply(world));
}
TEST_CASE("Inspector and serialization share the same Euler convention including poles") {
    for (const auto angles :
         {glm::vec3(25, 30, -40), glm::vec3(60, 90, 35), glm::vec3(-15, -90, 20)}) {
        const auto rotation = scene::rotationDegrees(angles);
        const auto copied = scene::rotationDegrees(scene::eulerDegrees(rotation));
        CHECK(std::abs(glm::dot(rotation, copied)) == doctest::Approx(1).epsilon(.00001));
    }
}
} // namespace somnium::debug_ui::detail
