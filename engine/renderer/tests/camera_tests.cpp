// Exercises the mathematical camera contract without a device or imported scene.
// Pixel tests independently cover view orientation, projection and world transforms.
#include <cmath>
#include <doctest/doctest.h>
#include <limits>
#include <somnium/renderer/renderer.hpp>

TEST_CASE("Camera is RH ZO with vertical FOV and target aspect") {
    using namespace somnium;
    renderer::Camera camera{{0, 0, 5}, {0, 0, 0}, {0, 1, 0}, 1.0F, 0.1F, 100.0F};
    auto frame = renderer::cameraFrame(camera, {800, 400});
    REQUIRE(frame);
    CHECK(frame->projection[1][1] == doctest::Approx(1.0F / std::tan(0.5F)));
    CHECK(frame->projection[0][0] * 2 == doctest::Approx(frame->projection[1][1]));
    auto near = frame->projection * glm::vec4(0, 0, -camera.nearPlane, 1);
    auto far = frame->projection * glm::vec4(0, 0, -camera.farPlane, 1);
    CHECK(near.z / near.w == doctest::Approx(0));
    CHECK(far.z / far.w == doctest::Approx(1));
    auto origin = frame->view * glm::vec4(0, 0, 0, 1);
    CHECK(origin.z == doctest::Approx(-5));
    CHECK_FALSE(renderer::cameraFrame(camera, {0, 400}));
    camera.target = camera.eye;
    CHECK_FALSE(renderer::cameraFrame(camera, {800, 400}));
    camera.target = {0, 0, 0};
    camera.up = {0, 0, 1};
    CHECK_FALSE(renderer::cameraFrame(camera, {800, 400}));
    camera.up = {0, 1, 0};
    camera.farPlane = camera.nearPlane;
    CHECK_FALSE(renderer::cameraFrame(camera, {800, 400}));
    camera.farPlane = std::numeric_limits<float>::infinity();
    CHECK_FALSE(renderer::cameraFrame(camera, {800, 400}));
}
