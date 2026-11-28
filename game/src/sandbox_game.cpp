// Updates collision, doors and the root follow camera from fixed-step input.
// Door geometry rotates under a pivot; the closed proxy remains until fully open.
#include "collision.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <somnium/core/assert.hpp>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>
#include <somnium/game/sandbox_game.hpp>
namespace somnium::game::SOMNIUM_GAME_GENERATION {
namespace {
auto invalid(std::string message) {
    return tl::unexpected(Error{ErrorCode::InvalidArgument, std::move(message)});
}
glm::quat opening(float progress) {
    return glm::angleAxis(glm::radians(90.F) * progress, glm::vec3(0, 1, 0));
}
entt::entity resolve(const scene::World& world, const std::string& id) {
    auto found = world.find(id);
    SOMNIUM_VERIFY(found);
    return *found;
}
} // namespace
Result<void> SandboxGame::start(scene::World& world, const runtime::StartContext& context) {
    const auto& mapping = context.mapping;
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
    m_character.clear();
    m_camera.clear();
    m_colliders.reserve(world.size());
    m_doors.reserve(world.size());
    m_blocks.reserve(world.size());
    auto& registry = world.registry();
    for (auto entity : world.traverse()) {
        if (registry.all_of<Character>(entity)) {
            if (!m_character.empty())
                return invalid("Exactly one Character is supported");
            m_character = *world.id(entity);
        }
        if (registry.all_of<AabbCollider>(entity)) {
            auto box = detail::bounds(world, entity);
            if (!box)
                return tl::unexpected(box.error());
            m_colliders.emplace_back(*world.id(entity));
        }
        if (auto* door = registry.try_get<Door>(entity)) {
            if (!registry.all_of<AabbCollider>(entity) || !registry.valid(door->pivot) ||
                registry.get<Hierarchy>(door->pivot).parent != entity)
                return invalid(std::string(*world.id(entity)) +
                               ": Door requires a collider and a direct child pivot");
            if (!door->posed) {
                door->closedRotation =
                    glm::normalize(registry.get<Transform>(door->pivot).current.rotation *
                                   glm::conjugate(opening(door->progress)));
                door->posed = true;
            }
            m_doors.emplace_back(*world.id(entity));
        }
    }
    if (m_character.empty() || !registry.all_of<AabbCollider>(resolve(world, m_character)))
        return invalid("Character requires a root AabbCollider");
    const auto visual = registry.get<Character>(resolve(world, m_character)).visual;
    if (!registry.valid(visual) ||
        registry.get<Hierarchy>(visual).parent != resolve(world, m_character))
        return invalid("Character visual must be its direct child");
    if (world.activeCamera == entt::null)
        return invalid("Active camera is required");
    m_camera = *world.id(world.activeCamera);
    if (!registry.valid(resolve(world, m_camera)) ||
        !registry.all_of<Camera, FollowCamera>(resolve(world, m_camera)) ||
        registry.get<Hierarchy>(resolve(world, m_camera)).parent != entt::null)
        return invalid("Active camera must be a root Camera with game.FollowCamera");
    auto& camera = registry.get<FollowCamera>(resolve(world, m_camera));
    if (camera.target != resolve(world, m_character))
        return invalid("FollowCamera target must be the Character");
    if (!camera.posed) {
        const auto target =
            registry.get<Transform>(resolve(world, m_character)).current.position + camera.offset;
        auto posed =
            world.teleport(resolve(world, m_camera), OrbitCamera(camera.orbit).pose(target));
        if (!posed)
            return posed;
        camera.posed = true;
    }
    m_tick = context.tick;
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
        const auto character =
            registry.get<Transform>(resolve(world, m_character)).current.position;
        for (const auto& id : m_doors) {
            const auto found = world.find(id);
            if (!found)
                continue;
            const auto entity = *found;
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
    for (const auto& id : m_doors) {
        const auto found = world.find(id);
        if (!found)
            continue;
        const auto entity = *found;
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
    for (const auto& id : m_colliders) {
        const auto found = world.find(id);
        if (!found)
            continue;
        const auto entity = *found;
        if (entity == resolve(world, m_character) || !registry.valid(entity))
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
    // Rotate the horizontal input plane by the current follow-camera yaw.
    // Pitch and zoom must not change movement speed or add vertical motion.
    const auto yaw = registry.get<FollowCamera>(resolve(world, m_camera)).orbit.yaw;
    direction = glm::angleAxis(yaw, glm::vec3(0, 1, 0)) * direction;
    const auto& character = registry.get<Character>(resolve(world, m_character));
    auto box = detail::bounds(world, resolve(world, m_character));
    if (!box)
        return tl::unexpected(box.error());
    float speed = character.speed;
#if defined(SOMNIUM_GAME_LAYOUT_V2)
    speed *= 2.F;
#endif
    auto movement =
        detail::move(*box, direction * (speed * static_cast<float>(update.seconds)), m_blocks);
    if (!movement)
        return tl::unexpected(movement.error());
    auto pose = registry.get<Transform>(resolve(world, m_character)).current;
    pose.position += *movement;
    auto changed = world.setLocal(resolve(world, m_character), pose);
    if (!changed)
        return changed;
    if (length > 0) {
        if (!registry.valid(character.visual))
            return invalid("Character visual was destroyed during gameplay");
        auto visual = registry.get<Transform>(character.visual).current;
        const auto facing = std::atan2(-direction.x, -direction.z);
        visual.rotation = glm::normalize(glm::conjugate(pose.rotation) *
                                         glm::angleAxis(facing, glm::vec3(0, 1, 0)));
        return world.setLocal(character.visual, visual);
    }
    return {};
}
void SandboxGame::updateOrbit(scene::World& world, const runtime::FixedUpdate& update) {
    auto& camera = world.registry().get<FollowCamera>(resolve(world, m_camera));
    const bool reset = update.inputs.action(m_orbit.reset).pressed;
    OrbitCamera orbit(camera.orbit);
    if (reset)
        orbit = OrbitCamera(camera.resetOrbit);
    else
        orbit.update(update.inputs, m_orbit, update.seconds);
    camera.orbit = orbit.state();
    camera.orbit.pitch = std::clamp(camera.orbit.pitch, glm::radians(5.F), glm::radians(75.F));
}
Result<void> SandboxGame::follow(scene::World& world, const runtime::FixedUpdate& update) {
    const auto& camera = world.registry().get<FollowCamera>(resolve(world, m_camera));
    const bool reset = update.inputs.action(m_orbit.reset).pressed;
    const auto target =
        world.registry().get<Transform>(camera.target).current.position + camera.offset;
    const auto pose = OrbitCamera(camera.orbit).pose(target);
    return reset ? world.teleport(resolve(world, m_camera), pose)
                 : world.setLocal(resolve(world, m_camera), pose);
}
Result<void> SandboxGame::fixedUpdate(scene::World& world, const runtime::FixedUpdate& update) {
    if (!m_running || update.tick != m_tick || update.inputs.step != update.tick ||
        !std::isfinite(update.seconds) || update.seconds <= 0)
        return invalid("Invalid gameplay lifecycle, tick or duration");
    if (!world.find(m_character) || !world.find(m_camera))
        return invalid("Gameplay character or camera was destroyed");
#if defined(SOMNIUM_GAME_LAYOUT_V2)
    SOMNIUM_PROFILE_SCOPE("game.v2.fixed_update");
#else
    SOMNIUM_PROFILE_SCOPE("game.fixed_update");
#endif
    auto doors = updateDoors(world, update);
    if (!doors)
        return doors;
    // Orbit and movement share this tick's input, then the camera follows the resolved pose.
    updateOrbit(world, update);
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
    if (world.find(m_character)) {
        const auto p = registry.get<Transform>(resolve(world, m_character)).current.position;
        Log::write("game", Log::Level::Info,
                   "GAMEPLAY_PLAYER position=({:.5f},{:.5f},{:.5f}) steps={}", p.x, p.y, p.z,
                   m_tick);
    }
    for (const auto& id : m_doors)
        if (const auto found = world.find(id)) {
            const auto entity = *found;
            const auto& door = registry.get<Door>(entity);
            Log::write("game", Log::Level::Info, "GAMEPLAY_DOOR id={} degrees={:.5f} open={}",
                       *world.id(entity), door.progress * 90.F, door.state == DoorState::Open);
        }
    if (const auto found = world.find(m_camera); found && registry.all_of<FollowCamera>(*found)) {
        const auto& camera = registry.get<FollowCamera>(*found);
        Log::write("game", Log::Level::Info,
                   "GAMEPLAY_CAMERA yaw={:.5f} pitch={:.5f} distance={:.5f}", camera.orbit.yaw,
                   camera.orbit.pitch, camera.orbit.distance);
    }
}
} // namespace somnium::game::SOMNIUM_GAME_GENERATION
