// Defines game-owned collision, door and third-person camera state.
// References use scene entities in memory and persistent IDs in the game codecs.
#pragma once
#include <somnium/game/generation.hpp>
#include <somnium/game/orbit_camera.hpp>
namespace somnium::game::SOMNIUM_GAME_GENERATION {
struct CollisionBounds {
    glm::vec3 minimum, maximum;
};
struct AabbCollider {
    glm::vec3 center{0};
    glm::vec3 halfExtent{.5F};
};
struct Character {
#if defined(SOMNIUM_GAME_LAYOUT_V2)
    std::array<std::uint64_t, 3> generationPadding{};
#endif
    float speed = 3.F;
    entt::entity visual = entt::null;
};
enum class DoorState : std::uint8_t { Closed, Opening, Open };
struct Door {
    entt::entity pivot = entt::null;
    float reach = 2.2F, duration = 1.F, progress = 0;
    DoorState state = DoorState::Closed;
    glm::quat closedRotation{1, 0, 0, 0};
    bool posed = false;
};
struct FollowCamera {
    entt::entity target = entt::null;
    glm::vec3 offset{0, 1.1F, 0};
    OrbitState orbit{.25F, .45F, 10.F};
    OrbitState resetOrbit{.25F, .45F, 10.F};
    bool posed = false;
};
} // namespace somnium::game::SOMNIUM_GAME_GENERATION
