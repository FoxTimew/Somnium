// Builds explicit right-handed, zero-to-one depth camera matrices.
// Aspect comes from the current target extent, including an acquired swapchain.
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <numbers>
#include <somnium/renderer/renderer.hpp>

namespace somnium::renderer {
Result<FrameData> cameraFrame(const Camera& camera, gpu::FrameSize extent) {
    const auto finite = [](glm::vec3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    const auto direction = camera.target - camera.eye;
    const auto right = glm::cross(direction, camera.up);
    if (!extent.drawable() || !finite(camera.eye) || !finite(camera.target) || !finite(camera.up) ||
        !finite(direction) || !finite(right) || !std::isfinite(glm::dot(direction, direction)) ||
        !std::isfinite(glm::dot(right, right)) || !std::isfinite(camera.verticalFov) ||
        camera.verticalFov <= 0 || camera.verticalFov >= std::numbers::pi_v<float> ||
        !std::isfinite(camera.nearPlane) || !std::isfinite(camera.farPlane) ||
        camera.nearPlane <= 0 || camera.farPlane <= camera.nearPlane ||
        glm::dot(direction, direction) < 1e-12F || glm::dot(right, right) < 1e-12F) {
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid camera or target extent"});
    }
    return FrameData{
        glm::lookAtRH(camera.eye, camera.target, camera.up),
        glm::perspectiveRH_ZO(camera.verticalFov,
                              static_cast<float>(extent.width) / static_cast<float>(extent.height),
                              camera.nearPlane, camera.farPlane)};
}
} // namespace somnium::renderer
