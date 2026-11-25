// Supplies only the sandbox's axis-aligned collision proxy and swept movement.
// It is game code, independent of rendering, platform and any physics engine.
#pragma once
#include <somnium/game/components.hpp>
namespace somnium::game::detail {
using Bounds = CollisionBounds;
[[nodiscard]] Result<Bounds> bounds(const scene::World& world, entt::entity entity);
[[nodiscard]] Result<glm::vec3> move(Bounds character, glm::vec3 displacement,
                                     std::span<const Bounds> obstacles);
} // namespace somnium::game::detail
