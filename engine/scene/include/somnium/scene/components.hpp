// Keeps scene data in flat components without renderer or platform dependencies.
// Only local TRS and logical identities persist; worlds and history are derived.
#pragma once
#include <entt/entity/entity.hpp>
#include <glm/gtc/quaternion.hpp>
#include <somnium/assets/model.hpp>

namespace somnium {
struct PersistentId {
    std::string value;
};
struct Name {
    std::string value;
};
struct Trs {
    glm::vec3 position{0};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 scale{1};
};
struct Transform {
    Trs previous, current;
    glm::mat4 world{1};
};
struct Hierarchy {
    entt::entity parent = entt::null;
    entt::entity firstChild = entt::null, lastChild = entt::null;
    entt::entity previousSibling = entt::null, nextSibling = entt::null;
};
struct MeshRenderer {
    assets::AssetHandle<assets::Model> model;
};
struct Camera {
    float verticalFov = 1.0471975512F;
    float nearPlane = .1F, farPlane = 100.F;
};
struct DirectionalLight {
    glm::vec3 direction{0, 0, 1};
};
struct SceneSettings {
    float ambient = 0;
    std::array<float, 4> clearColor{0, 0, 0, 1};
};
} // namespace somnium
