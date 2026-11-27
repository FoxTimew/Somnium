// Defines lossless v2 codecs and explicit migration from the v1 Euler file boundary.
// Hierarchy references use exactly the deferred resolver exposed to game codecs.
#include <algorithm>
#include <somnium/scene/rotation.hpp>
#include <somnium/scene/serialization.hpp>

namespace somnium::scene {
namespace {
template <typename T> bool contains(const World& world, entt::entity entity) {
    return world.registry().all_of<T>(entity);
}
Json vector(glm::vec3 value) {
    return Json::array({value.x, value.y, value.z});
}
Result<Json> writeName(const World& world, entt::entity entity) {
    return Json{{"value", world.registry().get<Name>(entity).value}};
}
Result<void> readName(World& world, entt::entity entity, const Json& data, DeserializeContext&) {
    auto fields = requireFields(data, {"value"});
    if (!fields)
        return fields;
    auto name = readString(data, "value");
    if (!name)
        return tl::unexpected(name.error());
    world.registry().get<Name>(entity).value = std::move(*name);
    return {};
}
Json writeTrs(const Trs& local) {
    return Json{{"position", vector(local.position)},
                {"rotation", Json::array({local.rotation.x, local.rotation.y, local.rotation.z,
                                          local.rotation.w})},
                {"scale", vector(local.scale)}};
}
Result<Trs> readTrs(const Json& data) {
    auto fields = requireFields(data, {"position", "rotation", "scale"});
    if (!fields)
        return tl::unexpected(fields.error());
    auto position = readVector3(data, "position");
    auto rotation = readQuaternion(data, "rotation");
    auto scale = readVector3(data, "scale");
    if (!position)
        return tl::unexpected(position.error());
    if (!rotation)
        return tl::unexpected(rotation.error());
    if (!scale)
        return tl::unexpected(scale.error());
    const Trs local{*position, *rotation, *scale};
    auto valid = validateTrs(local);
    if (!valid)
        return tl::unexpected(valid.error());
    return local;
}
Result<Json> writeTransform(const World& world, entt::entity entity) {
    return writeTrs(world.registry().get<Transform>(entity).current);
}
Result<void> readTransform(World& world, entt::entity entity, const Json& data,
                           DeserializeContext&) {
    auto local = readTrs(data);
    if (!local)
        return tl::unexpected(local.error());
    return world.teleport(entity, *local);
}
Result<Json> migrateTransform(std::uint32_t version, const Json& data) {
    if (version != 1)
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Unsupported Transform version"});
    auto fields = requireFields(data, {"position", "rotationDegrees", "scale"});
    if (!fields)
        return tl::unexpected(fields.error());
    auto degrees = readVector3(data, "rotationDegrees");
    if (!degrees)
        return tl::unexpected(degrees.error());
    Json migrated = data;
    migrated.erase("rotationDegrees");
    const auto q = rotationDegrees(*degrees);
    migrated["rotation"] = Json::array({q.x, q.y, q.z, q.w});
    return migrated;
}
Result<Json> captureTransform(const World& world, entt::entity entity) {
    return writeTrs(world.registry().get<Transform>(entity).previous);
}
Result<void> restoreTransform(World& world, entt::entity entity, const Json& data) {
    auto local = readTrs(data);
    if (!local)
        return tl::unexpected(local.error());
    world.registry().get<Transform>(entity).previous = *local;
    return {};
}
Result<Json> writeHierarchy(const World& world, entt::entity entity) {
    const auto parent = world.registry().get<Hierarchy>(entity).parent;
    if (parent == entt::null)
        return Json{{"parent", nullptr}};
    auto id = world.id(parent);
    if (!id)
        return tl::unexpected(id.error());
    return Json{{"parent", std::string(*id)}};
}
Result<void> readHierarchy(World&, entt::entity entity, const Json& data,
                           DeserializeContext& context) {
    auto fields = requireFields(data, {"parent"});
    if (!fields)
        return fields;
    if (data["parent"].is_null())
        return {};
    auto id = readString(data, "parent");
    if (!id)
        return tl::unexpected(id.error());
    context.deferEntity(entity, "somnium.Hierarchy/parent", std::move(*id),
                        [](World& world, entt::entity owner, entt::entity parent) {
                            return world.reparent(owner, parent);
                        });
    return {};
}
Result<Json> writeMesh(const World& world, entt::entity entity) {
    auto path = world.modelPath(world.registry().get<MeshRenderer>(entity).model);
    if (!path)
        return tl::unexpected(path.error());
    return Json{{"model", std::string(*path)}};
}
Result<void> readMesh(World& world, entt::entity entity, const Json& data,
                      DeserializeContext& context) {
    auto fields = requireFields(data, {"model"});
    if (!fields)
        return fields;
    auto path = readString(data, "model");
    if (!path)
        return tl::unexpected(path.error());
    auto handle = context.requestModel(*path);
    if (!handle)
        return tl::unexpected(handle.error());
    world.registry().emplace<MeshRenderer>(entity, *handle);
    return {};
}
Result<Json> writeCamera(const World& world, entt::entity entity) {
    const auto& camera = world.registry().get<Camera>(entity);
    return Json{{"verticalFovRadians", camera.verticalFov},
                {"near", camera.nearPlane},
                {"far", camera.farPlane}};
}
Result<void> readCamera(World& world, entt::entity entity, const Json& data, DeserializeContext&) {
    auto fields = requireFields(data, {"verticalFovRadians", "near", "far"});
    if (!fields)
        return fields;
    auto fov = readNumber(data, "verticalFovRadians");
    auto nearPlane = readNumber(data, "near");
    auto farPlane = readNumber(data, "far");
    if (!fov)
        return tl::unexpected(fov.error());
    if (!nearPlane)
        return tl::unexpected(nearPlane.error());
    if (!farPlane)
        return tl::unexpected(farPlane.error());
    world.registry().emplace<Camera>(entity, *fov, *nearPlane, *farPlane);
    return {};
}
Result<Json> migrateCamera(std::uint32_t version, const Json& data) {
    if (version != 1)
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Unsupported Camera version"});
    auto fields = requireFields(data, {"verticalFovDegrees", "near", "far"});
    if (!fields)
        return tl::unexpected(fields.error());
    auto fov = readNumber(data, "verticalFovDegrees");
    if (!fov)
        return tl::unexpected(fov.error());
    Json migrated = data;
    migrated.erase("verticalFovDegrees");
    migrated["verticalFovRadians"] = glm::radians(*fov);
    return migrated;
}
Result<Json> writeLight(const World& world, entt::entity entity) {
    return Json{{"direction", vector(world.registry().get<DirectionalLight>(entity).direction)}};
}
Result<void> readLight(World& world, entt::entity entity, const Json& data, DeserializeContext&) {
    auto fields = requireFields(data, {"direction"});
    if (!fields)
        return fields;
    auto direction = readVector3(data, "direction");
    if (!direction)
        return tl::unexpected(direction.error());
    world.registry().emplace<DirectionalLight>(entity, *direction);
    return {};
}
} // namespace
Result<void> registerBuiltinCodecs(CodecRegistry& codecs) {
    for (auto codec :
         {ComponentCodec{"somnium.Name", 1, contains<Name>, writeName, readName},
          ComponentCodec{"somnium.Transform", 2, contains<Transform>, writeTransform, readTransform,
                         migrateTransform, captureTransform, restoreTransform},
          ComponentCodec{"somnium.Hierarchy", 1, contains<Hierarchy>, writeHierarchy,
                         readHierarchy},
          ComponentCodec{"somnium.MeshRenderer", 1, contains<MeshRenderer>, writeMesh, readMesh},
          ComponentCodec{"somnium.Camera", 2, contains<Camera>, writeCamera, readCamera,
                         migrateCamera},
          ComponentCodec{"somnium.DirectionalLight", 1, contains<DirectionalLight>, writeLight,
                         readLight}}) {
#if defined(SOMNIUM_ENABLE_ITERATION)
        if (codec.name == "somnium.Name")
            codec.editor.initial = {{"value", "Entity"}};
        else if (codec.name == "somnium.Transform") {
            codec.editor.initial = writeTrs({});
            codec.editor.fields.emplace("/rotation", FieldPresentation::QuaternionEuler);
        } else if (codec.name == "somnium.Hierarchy")
            codec.editor.initial = {{"parent", nullptr}};
        else if (codec.name == "somnium.MeshRenderer")
            codec.editor.initial = {{"model", ""}};
        else if (codec.name == "somnium.Camera") {
            const Camera defaults;
            codec.editor.initial = {{"verticalFovRadians", defaults.verticalFov},
                                    {"near", defaults.nearPlane},
                                    {"far", defaults.farPlane}};
            codec.editor.fields.emplace("/verticalFovRadians", FieldPresentation::RadiansDegrees);
        } else if (codec.name == "somnium.DirectionalLight")
            codec.editor.initial = {{"direction", vector(DirectionalLight{}.direction)}};
#endif
        auto added = codecs.add(std::move(codec));
        if (!added)
            return added;
    }
    return {};
}
} // namespace somnium::scene
