// Converts the declared file/editor Euler order without changing simulation representation.
// At a pole the equivalent Z=0 decomposition keeps serialization stable.
#include <cmath>
#include <somnium/scene/rotation.hpp>
namespace somnium::scene {
glm::vec3 eulerDegrees(const glm::quat& rotation) {
    const auto matrix = glm::mat3_cast(glm::normalize(glm::dquat(rotation)));
    const auto cosine = std::hypot(matrix[0][0], matrix[0][1]);
    const auto y = std::atan2(-matrix[0][2], cosine);
    const bool regular = cosine > .000001;
    const auto x =
        regular ? std::atan2(matrix[1][2], matrix[2][2]) : std::atan2(-matrix[2][1], matrix[1][1]);
    const auto z = regular ? std::atan2(matrix[0][1], matrix[0][0]) : 0.0;
    return glm::vec3(glm::degrees(glm::dvec3{x, y, z}));
}
glm::quat rotationDegrees(glm::vec3 degrees) {
    const auto radians = glm::radians(degrees);
    return glm::normalize(glm::angleAxis(radians.z, glm::vec3(0, 0, 1)) *
                          glm::angleAxis(radians.y, glm::vec3(0, 1, 0)) *
                          glm::angleAxis(radians.x, glm::vec3(1, 0, 0)));
}
} // namespace somnium::scene
