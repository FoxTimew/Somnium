// Describes a render camera as values, without GPU resources or scene entities.
// Gameplay chooses the pose; renderer derives matrices from the target extent.
#pragma once
#include <glm/vec3.hpp>
namespace somnium::renderer {
struct Camera {
    glm::vec3 eye, target, up;
    float verticalFov, nearPlane, farPlane;
};
} // namespace somnium::renderer
