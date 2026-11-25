// Updates collision, doors and the root follow camera from fixed-step input.
// Door geometry rotates under a pivot; the closed proxy remains until fully open.
#include "collision.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>
#include <somnium/game/sandbox_game.hpp>
namespace somnium::game {
namespace {
auto invalid(std::string message) {
    return tl::unexpected(Error{ErrorCode::InvalidArgument, std::move(message)});
}
glm::quat opening(float progress) {
    return glm::angleAxis(glm::radians(90.F) * progress, glm::vec3(0, 1, 0));
}
} // namespace
Result<void> SandboxGame::start(scene::World& world, const input::Configuration& mapping) {
    if (m_running)
        return invalid("Game already started");
    auto orbit = OrbitControls::resolve(mapping);
    if (!orbit)
        return tl::unexpected(orbit.error());
    m_orbit = *orbit;
    auto interact = mapping.action("Interact");
    auto moveX = mapping.axis("MoveX"), moveZ = mapping.axis("MoveZ");
    if (!interact)
        return tl::unexpected(interact.error());
    if (!moveX)
        return tl::unexpected(moveX.error());
    if (!moveZ)
        return tl::unexpected(moveZ.error());
    m_interact = *interact;
    m_moveX = *moveX;
    m_moveZ = *moveZ;
    m_colliders.clear();
    m_doors.clear();
    m_blocks.clear();
    m_character = entt::null;
    m_colliders.reserve(world.size());
    m_doors.reserve(world.size());
    m_blocks.reserve(world.size());
    auto& registry = world.registry();
    for (auto entity : world.traverse()) {
        if (registry.all_of<Character>(entity)) {
            if (m_character != entt::null)
                return invalid("Exactly one Character is supported");
            m_character = entity;
        }
        if (registry.all_of<AabbCollider>(entity)) {
            auto box = detail::bounds(world, entity);
            if (!box)
                return tl::unexpected(box.error());
            m_colliders.push_back(entity);
        }
        if (auto* door = registry.try_get<Door>(entity)) {
            if (!registry.all_of<AabbCollider>(entity) || !registry.valid(door->pivot) ||
                registry.get<Hierarchy>(door->pivot).parent != entity)
                return invalid(std::string(*world.id(entity)) +
                               ": Door requires a collider and a direct child pivot");
            door->closedRotation =
                glm::normalize(registry.get<Transform>(door->pivot).current.rotation *
                               glm::conjugate(opening(door->progress)));
            m_doors.push_back(entity);
        }
    }
    if (m_character == entt::null || !registry.all_of<AabbCollider>(m_character))
        return invalid("Character requires a root AabbCollider");
    const auto visual = registry.get<Character>(m_character).visual;
    if (!registry.valid(visual) || registry.get<Hierarchy>(visual).parent != m_character)
        return invalid("Character visual must be its direct child");
    m_camera = world.activeCamera;
    if (!registry.valid(m_camera) || !registry.all_of<Camera, FollowCamera>(m_camera) ||
        registry.get<Hierarchy>(m_camera).parent != entt::null)
        return invalid("Active camera must be a root Camera with game.FollowCamera");
    auto& camera = registry.get<FollowCamera>(m_camera);
    if (camera.target != m_character)
        return invalid("FollowCamera target must be the Character");
    m_initialOrbit = camera.orbit;
    const auto target = registry.get<Transform>(m_character).current.position + camera.offset;
    auto posed = world.teleport(m_camera, OrbitCamera(camera.orbit).pose(target));
    if (!posed)
        return posed;
    m_tick = 0;
    m_running = true;
    Log::write("game", Log::Level::Info, "Third-person scene started: {} colliders, {} doors",
               m_colliders.size(), m_doors.size());
    return {};
}
Result<void> SandboxGame::updateDoors(scene::World& world, const runtime::FixedUpdate& update) {
    auto& registry = world.registry();
    if (update.inputs.action(m_interact).pressed) {
        entt::entity nearest = entt::null;
        float nearestDistance = std::numeric_limits<float>::infinity();
        const auto character = registry.get<Transform>(m_character).current.position;
        for (auto entity : m_doors) {
            if (!registry.valid(entity))
                continue;
            const auto& door = registry.get<Door>(entity);
            if (door.state != DoorState::Closed)
                continue;
            const auto delta = registry.get<Transform>(entity).current.position - character;
            const auto distance = glm::dot(delta, delta);
            if (distance <= door.reach * door.reach && distance < nearestDistance) {
                nearest = entity;
                nearestDistance = distance;
            }
        }
        if (nearest != entt::null)
            registry.get<Door>(nearest).state = DoorState::Opening;
    }
    for (auto entity : m_doors) {
        if (!registry.valid(entity))
            continue;
        auto& door = registry.get<Door>(entity);
        if (door.state != DoorState::Opening)
            continue;
        door.progress =
            std::min(1.F, door.progress + static_cast<float>(update.seconds) / door.duration);
        // Avoid a 61st tick solely due to accumulating float roundoff at 60 Hz.
        if (door.progress >= 1.F - 1.e-6F) {
            door.progress = 1;
            door.state = DoorState::Open;
        }
        if (!registry.valid(door.pivot))
            return invalid("Door pivot was destroyed during gameplay");
        auto pose = registry.get<Transform>(door.pivot).current;
        pose.rotation = glm::normalize(door.closedRotation * opening(door.progress));
        auto changed = world.setLocal(door.pivot, pose);
        if (!changed)
            return changed;
    }
    return {};
}
Result<void> SandboxGame::moveCharacter(scene::World& world, const runtime::FixedUpdate& update) {
    auto& registry = world.registry();
    m_blocks.clear();
    for (auto entity : m_colliders) {
        if (entity == m_character || !registry.valid(entity))
            continue;
        if (const auto* door = registry.try_get<Door>(entity);
            door && door->state == DoorState::Open)
            continue;
        auto box = detail::bounds(world, entity);
        if (!box)
            return tl::unexpected(box.error());
        m_blocks.push_back(*box);
    }
    glm::vec3 direction(update.inputs.axis(m_moveX), 0, update.inputs.axis(m_moveZ));
    if (!std::isfinite(direction.x) || !std::isfinite(direction.z))
        return invalid("Non-finite movement input");
    const auto length = glm::length(direction);
    if (length > 1)
        direction /= length;
    const auto& character = registry.get<Character>(m_character);
    auto box = detail::bounds(world, m_character);
    if (!box)
        return tl::unexpected(box.error());
    auto movement = detail::move(
        *box, direction * (character.speed * static_cast<float>(update.seconds)), m_blocks);
    if (!movement)
        return tl::unexpected(movement.error());
    auto pose = registry.get<Transform>(m_character).current;
    pose.position += *movement;
    auto changed = world.setLocal(m_character, pose);
    if (!changed)
        return changed;
    if (length > 0) {
        if (!registry.valid(character.visual))
            return invalid("Character visual was destroyed during gameplay");
        auto visual = registry.get<Transform>(character.visual).current;
        const auto yaw = std::atan2(-direction.x, -direction.z);
        visual.rotation =
            glm::normalize(glm::conjugate(pose.rotation) * glm::angleAxis(yaw, glm::vec3(0, 1, 0)));
        return world.setLocal(character.visual, visual);
    }
    return {};
}
Result<void> SandboxGame::follow(scene::World& world, const runtime::FixedUpdate& update) {
    auto& camera = world.registry().get<FollowCamera>(m_camera);
    const bool reset = update.inputs.action(m_orbit.reset).pressed;
    OrbitCamera orbit(camera.orbit);
    if (reset)
        orbit = OrbitCamera(m_initialOrbit);
    else
        orbit.update(update.inputs, m_orbit, update.seconds);
    camera.orbit = orbit.state();
    camera.orbit.pitch = std::clamp(camera.orbit.pitch, glm::radians(5.F), glm::radians(75.F));
    const auto target =
        world.registry().get<Transform>(camera.target).current.position + camera.offset;
    const auto pose = OrbitCamera(camera.orbit).pose(target);
    return reset ? world.teleport(m_camera, pose) : world.setLocal(m_camera, pose);
}
Result<void> SandboxGame::fixedUpdate(scene::World& world, const runtime::FixedUpdate& update) {
    if (!m_running || update.tick != m_tick || update.inputs.step != update.tick ||
        !std::isfinite(update.seconds) || update.seconds <= 0)
        return invalid("Invalid gameplay lifecycle, tick or duration");
    auto& registry = world.registry();
    if (!registry.valid(m_character) || !registry.valid(m_camera))
        return invalid("Gameplay character or camera was destroyed");
    SOMNIUM_PROFILE_SCOPE("game.fixed_update");
    auto doors = updateDoors(world, update);
    if (!doors)
        return doors;
    auto movement = moveCharacter(world, update);
    if (!movement)
        return movement;
    auto camera = follow(world, update);
    if (!camera)
        return camera;
    ++m_tick;
    return {};
}
void SandboxGame::stop(scene::World& world) {
    m_running = false;
    const auto& registry = world.registry();
    if (registry.valid(m_character)) {
        const auto p = registry.get<Transform>(m_character).current.position;
        Log::write("game", Log::Level::Info,
                   "GAMEPLAY_PLAYER position=({:.5f},{:.5f},{:.5f}) steps={}", p.x, p.y, p.z,
                   m_tick);
    }
    for (auto entity : m_doors)
        if (registry.valid(entity)) {
            const auto& door = registry.get<Door>(entity);
            Log::write("game", Log::Level::Info, "GAMEPLAY_DOOR id={} degrees={:.5f} open={}",
                       *world.id(entity), door.progress * 90.F, door.state == DoorState::Open);
        }
    if (registry.valid(m_camera)) {
        const auto& camera = registry.get<FollowCamera>(m_camera);
        Log::write("game", Log::Level::Info,
                   "GAMEPLAY_CAMERA yaw={:.5f} pitch={:.5f} distance={:.5f}", camera.orbit.yaw,
                   camera.orbit.pitch, camera.orbit.distance);
    }
}
} // namespace somnium::game
