// Resolves horizontal overlap then sweeps X and Z separately to retain wall sliding.
// Sweeps clamp at entry planes regardless of displacement, preventing tunnelling.
#include "collision.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace somnium::game::SOMNIUM_GAME_GENERATION::detail {
namespace {
bool overlap(float amin, float amax, float bmin, float bmax) {
    return amin < bmax && amax > bmin;
}
bool intersects(const Bounds& a, const Bounds& b) {
    return overlap(a.minimum.x, a.maximum.x, b.minimum.x, b.maximum.x) &&
           overlap(a.minimum.y, a.maximum.y, b.minimum.y, b.maximum.y) &&
           overlap(a.minimum.z, a.maximum.z, b.minimum.z, b.maximum.z);
}
void translate(Bounds& box, glm::vec3 delta) {
    box.minimum += delta;
    box.maximum += delta;
}
} // namespace
Result<Bounds> bounds(const scene::World& world, entt::entity entity) {
    const auto& registry = world.registry();
    if (!registry.valid(entity) || !registry.all_of<AabbCollider, Transform, Hierarchy>(entity))
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Collision entity is missing components"});
    if (registry.get<Hierarchy>(entity).parent != entt::null)
        return tl::unexpected(Error{ErrorCode::InvalidArgument,
                                    std::string(*world.id(entity)) + ": collider must be a root"});
    const auto& collider = registry.get<AabbCollider>(entity);
    const auto matrix = scene::localMatrix(registry.get<Transform>(entity).current);
    const auto center = glm::vec3(matrix * glm::vec4(collider.center, 1));
    const auto half = glm::abs(glm::vec3(matrix[0])) * collider.halfExtent.x +
                      glm::abs(glm::vec3(matrix[1])) * collider.halfExtent.y +
                      glm::abs(glm::vec3(matrix[2])) * collider.halfExtent.z;
    const Bounds result{center - half, center + half};
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(result.minimum[axis]) || !std::isfinite(result.maximum[axis]) ||
            collider.halfExtent[axis] <= 0)
            return tl::unexpected(
                Error{ErrorCode::InvalidArgument,
                      std::string(*world.id(entity)) + ": invalid collider bounds"});
    return result;
}
Result<glm::vec3> move(Bounds character, glm::vec3 displacement,
                       std::span<const Bounds> obstacles) {
    glm::vec3 movement(0);
    // Inspector teleports can start inside a volume: use the shortest horizontal exit.
    // Fixed order and a bounded number of passes make this deterministic, including failure.
    for (std::size_t pass = 0; pass <= obstacles.size(); ++pass) {
        bool adjusted = false;
        for (const auto& obstacle : obstacles) {
            if (!intersects(character, obstacle))
                continue;
            glm::vec3 correction(0);
            float shortest = std::numeric_limits<float>::infinity();
            for (int axis : {0, 2}) {
                for (float distance : {obstacle.minimum[axis] - character.maximum[axis],
                                       obstacle.maximum[axis] - character.minimum[axis]}) {
                    if (std::abs(distance) < std::abs(shortest)) {
                        shortest = distance;
                        correction = glm::vec3(0);
                        correction[axis] = distance;
                    }
                }
            }
            translate(character, correction);
            movement += correction;
            adjusted = true;
        }
        if (!adjusted)
            break;
        if (pass == obstacles.size())
            return tl::unexpected(
                Error{ErrorCode::InvalidState,
                      "Overlapping collision volumes have no resolved horizontal exit"});
    }
    for (int axis : {0, 2}) {
        float distance = displacement[axis];
        if (!std::isfinite(distance))
            return tl::unexpected(
                Error{ErrorCode::InvalidArgument, "Non-finite character displacement"});
        const int other = axis == 0 ? 2 : 0;
        for (const auto& obstacle : obstacles) {
            if (!overlap(character.minimum.y, character.maximum.y, obstacle.minimum.y,
                         obstacle.maximum.y) ||
                !overlap(character.minimum[other], character.maximum[other],
                         obstacle.minimum[other], obstacle.maximum[other]))
                continue;
            if (distance > 0 && character.maximum[axis] <= obstacle.minimum[axis])
                distance = std::min(distance, obstacle.minimum[axis] - character.maximum[axis]);
            else if (distance < 0 && character.minimum[axis] >= obstacle.maximum[axis])
                distance = std::max(distance, obstacle.maximum[axis] - character.minimum[axis]);
        }
        glm::vec3 delta(0);
        delta[axis] = distance;
        translate(character, delta);
        movement += delta;
    }
    return movement;
}
} // namespace somnium::game::SOMNIUM_GAME_GENERATION::detail
