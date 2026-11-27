// Checks hierarchy interpolation with independent expected positions and camera poses.
// No device is needed: renderer handles are opaque identities in these CPU tests.
#include "scene_rendering.hpp"
#include <doctest/doctest.h>
#include <glm/ext/matrix_transform.hpp>
#include <limits>
#include <numbers>

namespace {
using namespace somnium;
entt::entity create(scene::World& world, std::string_view id) {
    auto value = world.create(id);
    REQUIRE(value);
    return *value;
}
void near(const glm::mat4& actual, const glm::mat4& expected) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            CHECK(actual[c][r] == doctest::Approx(expected[c][r]).epsilon(.0001));
}
} // namespace
TEST_CASE("Local interpolation follows a pivot arc and recomposes glTF instances") {
    scene::World world;
    const auto pivot = create(world, "pivot"), child = create(world, "child"),
               camera = create(world, "camera");
    world.registry().emplace<Camera>(camera);
    world.activeCamera = camera;
    REQUIRE(world.reparent(child, pivot));
    REQUIRE(world.reparent(camera, pivot));
    REQUIRE(world.teleport(child, {.position = {2, 0, 0}, .scale = {2, 1, 1}}));
    REQUIRE(world.teleport(camera, {.position = {0, 0, 10}}));
    const assets::AssetHandle<assets::Model> model{3, 4};
    world.registry().emplace<MeshRenderer>(child, model);
    const std::array draws{
        renderer::Draw{{1, 2}, {3, 4}, glm::translate(glm::mat4(1), glm::vec3(0, 1, 0))}};
    const std::array models{runtime::ModelDraws{model, draws}};
    world.beginStep();
    const auto rotation = glm::angleAxis(std::numbers::pi_v<float> / 2, glm::vec3(0, 1, 0));
    REQUIRE(world.setLocal(pivot, {.rotation = rotation}));
    REQUIRE(world.propagate());
    runtime::SceneFrame frame;
    frame.reserve({.entities = world.size(), .draws = 1});
    const auto* storage = frame.worlds.data();
    for (float alpha : {0.F, .5F, 1.F}) {
        REQUIRE(runtime::extractScene(world, alpha, {800, 400}, models, frame));
        const auto pivotPose =
            glm::rotate(glm::mat4(1), alpha * std::numbers::pi_v<float> / 2, glm::vec3(0, 1, 0));
        REQUIRE(frame.draws.size() == 1);
        near(frame.draws[0].world,
             pivotPose * scene::localMatrix(world.registry().get<Transform>(child).current) *
                 draws[0].world);
        near(glm::inverse(frame.uniforms.view),
             pivotPose * scene::localMatrix(world.registry().get<Transform>(camera).current));
        CHECK(frame.draws[0].mesh == draws[0].mesh);
        CHECK(frame.draws[0].material == draws[0].material);
        CHECK(frame.uniforms.projection[1][1] / frame.uniforms.projection[0][0] ==
              doctest::Approx(2));
        if (alpha == .5F)
            CHECK(frame.draws[0].world[3].x == doctest::Approx(std::sqrt(2.F)));
    }
    CHECK(frame.worlds.data() == storage);
    REQUIRE(world.teleport(pivot, {.position = {5, 0, 0}, .rotation = rotation}));
    REQUIRE(runtime::extractScene(world, 0, {800, 400}, models, frame));
    near(frame.draws[0].world,
         scene::localMatrix(world.registry().get<Transform>(pivot).current) *
             scene::localMatrix(world.registry().get<Transform>(child).current) * draws[0].world);
    CHECK_FALSE(runtime::extractScene(world, -.1F, {800, 400}, models, frame));
    CHECK_FALSE(runtime::extractScene(world, std::numeric_limits<float>::quiet_NaN(), {800, 400},
                                      models, frame));
    CHECK_FALSE(runtime::extractScene(world, .5F, {0, 400}, models, frame));
    CHECK_FALSE(runtime::extractScene(world, .5F, {800, 400}, {}, frame));
}
TEST_CASE("Shortest quaternion interpolation, camera locals and light share scene time") {
    scene::World world;
    const auto camera = create(world, "camera"), light = create(world, "light");
    world.registry().emplace<Camera>(camera);
    world.registry().emplace<DirectionalLight>(light);
    world.activeCamera = camera;
    world.settings.ambient = .15F;
    world.settings.clearColor = {.1F, .2F, .3F, 1};
    const auto q = glm::angleAxis(.5F, glm::vec3(0, 1, 0));
    REQUIRE(world.teleport(camera, {.rotation = q}));
    world.beginStep();
    REQUIRE(world.setLocal(camera, {.position = {2, 4, 6}, .rotation = -q}));
    REQUIRE(world.setLocal(
        light, {.rotation = glm::angleAxis(std::numbers::pi_v<float> / 2, glm::vec3(0, 1, 0))}));
    runtime::SceneFrame frame;
    CHECK_FALSE(runtime::extractScene(world, .5F, {100, 100}, {}, frame));
    frame.reserve({.entities = world.size(), .draws = 0});
    REQUIRE(runtime::extractScene(world, .5F, {100, 100}, {}, frame));
    near(glm::inverse(frame.uniforms.view),
         scene::localMatrix({.position = {1, 2, 3}, .rotation = q}));
    CHECK(frame.uniforms.lightDirection.x == doctest::Approx(std::sqrt(.5F)));
    CHECK(frame.uniforms.lightDirection.z == doctest::Approx(std::sqrt(.5F)));
    CHECK(frame.uniforms.ambient == .15F);
    CHECK(frame.clearColor == world.settings.clearColor);
    world.activeCamera = entt::null;
    CHECK_FALSE(runtime::extractScene(world, .5F, {100, 100}, {}, frame));
}
TEST_CASE("Shared models expand every primitive instance and destroyed entities disappear") {
    scene::World world;
    const auto camera = create(world, "camera"), a = create(world, "a"), b = create(world, "b");
    world.registry().emplace<Camera>(camera);
    world.activeCamera = camera;
    const assets::AssetHandle<assets::Model> handle{2, 7};
    world.registry().emplace<MeshRenderer>(a, handle);
    world.registry().emplace<MeshRenderer>(b, handle);
    REQUIRE(world.teleport(a, {.position = {3, 0, 0}}));
    REQUIRE(world.teleport(b, {.position = {-3, 0, 0}}));
    const std::array draws{
        renderer::Draw{{1, 1}, {2, 1}, glm::mat4(1)},
        renderer::Draw{{3, 1}, {4, 1}, glm::translate(glm::mat4(1), glm::vec3(0, 2, 0))}};
    const std::array models{runtime::ModelDraws{handle, draws}};
    runtime::SceneFrame frame;
    frame.reserve({.entities = world.size(), .draws = 1});
    CHECK_FALSE(runtime::extractScene(world, 1, {100, 100}, models, frame));
    frame.reserve({.entities = world.size(), .draws = 4});
    REQUIRE(runtime::extractScene(world, 1, {100, 100}, models, frame));
    REQUIRE(frame.draws.size() == 4);
    CHECK(frame.draws[0].world[3].x == 3);
    CHECK(frame.draws[1].world[3].y == 2);
    CHECK(frame.draws[2].world[3].x == -3);
    CHECK(frame.draws[3].material == draws[1].material);
    REQUIRE(world.destroy(a));
    REQUIRE(runtime::extractScene(world, 1, {100, 100}, models, frame));
    REQUIRE(frame.draws.size() == 2);
    CHECK(frame.draws[0].world[3].x == -3);
    CHECK_FALSE(runtime::extractScene(world, 1.1F, {100, 100}, models, frame));
}
