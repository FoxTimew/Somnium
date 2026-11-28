// Serializes only authored/persistent game state, never native entity values.
// All references are resolved by the same second pass used by scene hierarchy.
#include <glm/common.hpp>
#include <numbers>
#include <somnium/game/codecs.hpp>
#include <somnium/game/components.hpp>
namespace somnium::game::SOMNIUM_GAME_GENERATION {
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
    auto& character = world.registry().emplace<Character>(entity);
    character.speed = *speed;
    character.visual = entt::null;
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
                {"state", state},
                {"closedRotation", Json::array({value.closedRotation.x, value.closedRotation.y,
                                                value.closedRotation.z, value.closedRotation.w})},
                {"posed", value.posed}};
}
Result<void> readDoor(World& world, entt::entity entity, const Json& data,
                      DeserializeContext& context) {
    auto fields = requireFields(
        data, {"pivot", "reach", "duration", "progress", "state", "closedRotation", "posed"});
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
    auto closed = readQuaternion(data, "closedRotation");
    if (!closed)
        return tl::unexpected(closed.error());
    if (!data["posed"].is_boolean())
        return invalid("posed must be boolean");
    world.registry().emplace<Door>(entity, entt::null, *reach, *duration, *progress, motion,
                                   *closed, data["posed"].get<bool>());
    context.deferEntity(
        entity, "game.Door/pivot", std::move(*pivot),
        [](World& target, entt::entity owner, entt::entity resolved) -> Result<void> {
            target.registry().get<Door>(owner).pivot = resolved;
            return {};
        });
    return {};
}
Result<Json> migrateDoor(std::uint32_t version, const Json& data) {
    if (version != 1)
        return invalid("Unsupported Door version");
    auto fields = requireFields(data, {"pivot", "reach", "duration", "progress", "state"});
    if (!fields)
        return tl::unexpected(fields.error());
    auto migrated = data;
    migrated["closedRotation"] = {0, 0, 0, 1};
    migrated["posed"] = false;
    return migrated;
}
Json writeOrbit(const OrbitState& value) {
    return {{"yawRadians", value.yaw}, {"pitchRadians", value.pitch}, {"distance", value.distance}};
}
Result<OrbitState> readOrbit(const Json& data) {
    auto fields = requireFields(data, {"yawRadians", "pitchRadians", "distance"});
    if (!fields)
        return tl::unexpected(fields.error());
    auto yaw = readNumber(data, "yawRadians"), pitch = readNumber(data, "pitchRadians"),
         distance = readNumber(data, "distance");
    if (!yaw)
        return tl::unexpected(yaw.error());
    if (!pitch)
        return tl::unexpected(pitch.error());
    if (!distance)
        return tl::unexpected(distance.error());
    if (*pitch < glm::radians(5.F) || *pitch > glm::radians(75.F) || *distance < 4 ||
        *distance > 30 || *yaw < -std::numbers::pi_v<float> || *yaw > std::numbers::pi_v<float>)
        return invalid("camera yaw [-pi,pi], pitch [5,75] degrees, distance [4,30] required");
    return OrbitState{*yaw, *pitch, *distance};
}
Result<Json> writeCamera(const World& world, entt::entity entity) {
    const auto& value = world.registry().get<FollowCamera>(entity);
    auto target = world.id(value.target);
    if (!target)
        return tl::unexpected(target.error());
    return Json{{"target", std::string(*target)},
                {"offset", vector(value.offset)},
                {"orbit", writeOrbit(value.orbit)},
                {"resetOrbit", writeOrbit(value.resetOrbit)},
                {"posed", value.posed}};
}
Result<void> readCamera(World& world, entt::entity entity, const Json& data,
                        DeserializeContext& context) {
    auto fields = requireFields(data, {"target", "offset", "orbit", "resetOrbit", "posed"});
    if (!fields)
        return fields;
    auto target = readString(data, "target");
    auto offset = readVector3(data, "offset");
    auto orbit = readOrbit(data["orbit"]), reset = readOrbit(data["resetOrbit"]);
    if (!target)
        return tl::unexpected(target.error());
    if (!offset)
        return tl::unexpected(offset.error());
    if (!orbit)
        return tl::unexpected(orbit.error());
    if (!reset)
        return tl::unexpected(reset.error());
    if (!data["posed"].is_boolean())
        return invalid("posed must be boolean");
    world.registry().emplace<FollowCamera>(entity, entt::null, *offset, *orbit, *reset,
                                           data["posed"].get<bool>());
    context.deferEntity(
        entity, "game.FollowCamera/target", std::move(*target),
        [](World& destination, entt::entity owner, entt::entity resolved) -> Result<void> {
            destination.registry().get<FollowCamera>(owner).target = resolved;
            return {};
        });
    return {};
}
Result<Json> migrateCamera(std::uint32_t version, const Json& data) {
    if (version != 1)
        return invalid("Unsupported FollowCamera version");
    auto fields =
        requireFields(data, {"target", "offset", "yawDegrees", "pitchDegrees", "distance"});
    if (!fields)
        return tl::unexpected(fields.error());
    auto yaw = readNumber(data, "yawDegrees"), pitch = readNumber(data, "pitchDegrees"),
         distance = readNumber(data, "distance");
    if (!yaw)
        return tl::unexpected(yaw.error());
    if (!pitch)
        return tl::unexpected(pitch.error());
    if (!distance)
        return tl::unexpected(distance.error());
    auto migrated = data;
    migrated.erase("yawDegrees");
    migrated.erase("pitchDegrees");
    migrated.erase("distance");
    migrated["orbit"] = writeOrbit({glm::radians(*yaw), glm::radians(*pitch), *distance});
    migrated["resetOrbit"] = migrated["orbit"];
    migrated["posed"] = false;
    return migrated;
}
} // namespace
Result<void> registerCodecs(scene::CodecRegistry& table) {
    for (auto codec :
         {scene::ComponentCodec{"game.AabbCollider", 1, contains<AabbCollider>, writeCollider,
                                readCollider},
          scene::ComponentCodec{"game.Character", 1, contains<Character>, writeCharacter,
                                readCharacter},
          scene::ComponentCodec{"game.Door", 2, contains<Door>, writeDoor, readDoor, migrateDoor},
          scene::ComponentCodec{"game.FollowCamera", 2, contains<FollowCamera>, writeCamera,
                                readCamera, migrateCamera}}) {
#if defined(SOMNIUM_ENABLE_ITERATION)
        if (codec.name == "game.AabbCollider")
            codec.editor.initial = {{"center", {0, 0, 0}}, {"halfExtent", {.5, .5, .5}}};
        else if (codec.name == "game.Character")
            codec.editor.initial = {{"speed", 3}, {"visual", ""}};
        else if (codec.name == "game.Door") {
            codec.editor.initial = {{"pivot", ""},
                                    {"reach", 2},
                                    {"duration", 1},
                                    {"progress", 0},
                                    {"state", "closed"},
                                    {"posed", false},
                                    {"closedRotation", {0, 0, 0, 1}}};
            codec.editor.fields.emplace("/closedRotation",
                                        scene::FieldPresentation::QuaternionEuler);
        } else if (codec.name == "game.FollowCamera") {
            const auto orbit = writeOrbit(OrbitState{});
            codec.editor.initial = {{"target", ""},
                                    {"offset", {0, 1, 0}},
                                    {"orbit", orbit},
                                    {"resetOrbit", orbit},
                                    {"posed", false}};
            for (auto path : {"/orbit/yawRadians", "/orbit/pitchRadians", "/resetOrbit/yawRadians",
                              "/resetOrbit/pitchRadians"})
                codec.editor.fields.emplace(path, scene::FieldPresentation::RadiansDegrees);
        }
#endif
        auto result = table.add(std::move(codec));
        if (!result)
            return result;
    }
    return {};
}
} // namespace somnium::game::SOMNIUM_GAME_GENERATION
