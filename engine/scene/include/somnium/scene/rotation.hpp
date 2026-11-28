// Shares the degrees/Rz*Ry*Rx boundary between serialization and editing tools.
// Simulation stores normalized quaternions rather than Euler angles.
#pragma once
#include <glm/gtc/quaternion.hpp>
#include <somnium/core/export.hpp>
namespace somnium::scene {
[[nodiscard]] SOMNIUM_ENGINE_API glm::vec3 eulerDegrees(const glm::quat& rotation);
[[nodiscard]] SOMNIUM_ENGINE_API glm::quat rotationDegrees(glm::vec3 degrees);
} // namespace somnium::scene
