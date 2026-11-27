// Serializes only authored/persistent game state, never native entity values.
// All references are resolved by the same second pass used by scene hierarchy.
#include <glm/common.hpp>
#include <numbers>
#include <somnium/game/codecs.hpp>
#include <somnium/game/components.hpp>
namespace somnium::game {
namespace {
using namespace scene;
template <typename T> bool contains(const World& world, entt::entity entity) {
    return world.registry().all_of<T>(entity);
}
Json vector(glm::vec3 value) {
    return Json::array({value.x, value.y, value.z});
}
auto invalid(std::string message) {
    return tl::unexpected(Error{ErrorCode::InvalidArgument, std::move(message)});
}
Result<Json> writeCollider(const World& world, entt::entity entity) {
    const auto& value = world.registry().get<AabbCollider>(entity);
    return Json{{"center", vector(value.center)}, {"halfExtent", vector(value.halfExtent)}};
}
Result<void> readCollider(World& world, entt::entity entity, const Json& data,
                          DeserializeContext&) {
    auto fields = requireFields(data, {"center", "halfExtent"});
    if (!fields)
        return fields;
    auto center = readVector3(data, "center"), half = readVector3(data, "halfExtent");
    if (!center)
        return tl::unexpected(center.error());
    if (!half)
        return tl::unexpected(half.error());
    if (half->x <= 0 || half->y <= 0 || half->z <= 0)
        return invalid("halfExtent must be strictly positive");
    world.registry().emplace<AabbCollider>(entity, *center, *half);
    return {};
}
Result<Json> writeCharacter(const World& world, entt::entity entity) {
    const auto& value = world.registry().get<Character>(entity);
    auto visual = world.id(value.visual);
    if (!visual)
        return tl::unexpected(visual.error());
    return Json{{"speed", value.speed}, {"visual", std::string(*visual)}};
}
Result<void> readCharacter(World& world, entt::entity entity, const Json& data,
                           DeserializeContext& context) {
    auto fields = requireFields(data, {"speed", "visual"});
    if (!fields)
        return fields;
    auto speed = readNumber(data, "speed");
    auto visual = readString(data, "visual");
    if (!speed)
        return tl::unexpected(speed.error());
    if (!visual)
        return tl::unexpected(visual.error());
    if (*speed <= 0)
        return invalid("speed must be positive");
    world.registry().emplace<Character>(entity, *speed, entt::null);
    context.deferEntity(
        entity, "game.Character/visual", std::move(*visual),
        [](World& target, entt::entity owner, entt::entity resolved) -> Result<void> {
            target.registry().get<Character>(owner).visual = resolved;
            return {};
        });
    return {};
}
Result<Json> writeDoor(const World& world, entt::entity entity) {
    const auto& value = world.registry().get<Door>(entity);
    auto pivot = world.id(value.pivot);
    if (!pivot)
        return tl::unexpected(pivot.error());
    const auto state = value.state == DoorState::Closed    ? "closed"
                       : value.state == DoorState::Opening ? "opening"
                                                           : "open";
    return Json{{"pivot", std::string(*pivot)},
                {"reach", value.reach},
                {"duration", value.duration},
                {"progress", value.progress},
                {"state", state}};
}
Result<void> readDoor(World& world, entt::entity entity, const Json& data,
                      DeserializeContext& context) {
    auto fields = requireFields(data, {"pivot", "reach", "duration", "progress", "state"});
    if (!fields)
        return fields;
    auto pivot = readString(data, "pivot"), state = readString(data, "state");
    auto reach = readNumber(data, "reach"), duration = readNumber(data, "duration"),
         progress = readNumber(data, "progress");
    if (!pivot)
        return tl::unexpected(pivot.error());
    if (!state)
        return tl::unexpected(state.error());
    if (!reach)
        return tl::unexpected(reach.error());
    if (!duration)
        return tl::unexpected(duration.error());
    if (!progress)
        return tl::unexpected(progress.error());
    if (*reach <= 0 || *duration <= 0 || *progress < 0 || *progress > 1)
        return invalid("door reach/duration/progress out of range");
    DoorState motion;
    if (*state == "closed" && *progress == 0)
        motion = DoorState::Closed;
    else if (*state == "opening" && *progress < 1)
        motion = DoorState::Opening;
    else if (*state == "open" && *progress == 1)
        motion = DoorState::Open;
    else
        return invalid("door state and progress disagree");
    world.registry().emplace<Door>(entity, entt::null, *reach, *duration, *progress, motion,
                                   glm::quat(1, 0, 0, 0));
    context.deferEntity(
        entity, "game.Door/pivot", std::move(*pivot),
        [](World& target, entt::entity owner, entt::entity resolved) -> Result<void> {
            target.registry().get<Door>(owner).pivot = resolved;
            return {};
        });
    return {};
}
Result<Json> writeCamera(const World& world, entt::entity entity) {
    const auto& value = world.registry().get<FollowCamera>(entity);
    auto target = world.id(value.target);
    if (!target)
        return tl::unexpected(target.error());
    return Json{{"target", std::string(*target)},
                {"offset", vector(value.offset)},
                {"yawDegrees", glm::degrees(value.orbit.yaw)},
                {"pitchDegrees", glm::degrees(value.orbit.pitch)},
                {"distance", value.orbit.distance}};
}
Result<void> readCamera(World& world, entt::entity entity, const Json& data,
                        DeserializeContext& context) {
    auto fields =
        requireFields(data, {"target", "offset", "yawDegrees", "pitchDegrees", "distance"});
    if (!fields)
        return fields;
    auto target = readString(data, "target");
    auto offset = readVector3(data, "offset");
    auto yaw = readNumber(data, "yawDegrees"), pitch = readNumber(data, "pitchDegrees"),
         distance = readNumber(data, "distance");
    if (!target)
        return tl::unexpected(target.error());
    if (!offset)
        return tl::unexpected(offset.error());
    if (!yaw)
        return tl::unexpected(yaw.error());
    if (!pitch)
        return tl::unexpected(pitch.error());
    if (!distance)
        return tl::unexpected(distance.error());
    if (*pitch < 5 || *pitch > 75 || *distance < 4 || *distance > 30 || *yaw < -180 || *yaw > 180)
        return invalid("camera yaw [-180,180], pitch [5,75], distance [4,30] required");
    world.registry().emplace<FollowCamera>(
        entity, entt::null, *offset,
        OrbitState{glm::radians(*yaw), glm::radians(*pitch), *distance});
    context.deferEntity(
        entity, "game.FollowCamera/target", std::move(*target),
        [](World& destination, entt::entity owner, entt::entity resolved) -> Result<void> {
            destination.registry().get<FollowCamera>(owner).target = resolved;
            return {};
        });
    return {};
}
} // namespace
Result<void> registerCodecs(scene::CodecRegistry& table) {
    for (auto codec : {scene::ComponentCodec{"game.AabbCollider", 1, contains<AabbCollider>,
                                             writeCollider, readCollider},
                       scene::ComponentCodec{"game.Character", 1, contains<Character>,
                                             writeCharacter, readCharacter},
                       scene::ComponentCodec{"game.Door", 1, contains<Door>, writeDoor, readDoor},
                       scene::ComponentCodec{"game.FollowCamera", 1, contains<FollowCamera>,
                                             writeCamera, readCamera}}) {
        auto result = table.add(std::move(codec));
        if (!result)
            return result;
    }
    return {};
}
} // namespace somnium::game
