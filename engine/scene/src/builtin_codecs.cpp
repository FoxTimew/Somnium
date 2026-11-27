// Defines the version-one codecs and the explicit degrees/Rz*Ry*Rx file boundary.
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
Result<Json> writeTransform(const World& world, entt::entity entity) {
    const auto& local = world.registry().get<Transform>(entity).current;
    return Json{{"position", vector(local.position)},
                {"rotationDegrees", vector(eulerDegrees(local.rotation))},
                {"scale", vector(local.scale)}};
}
Result<void> readTransform(World& world, entt::entity entity, const Json& data,
                           DeserializeContext&) {
    auto fields = requireFields(data, {"position", "rotationDegrees", "scale"});
    if (!fields)
        return fields;
    auto position = readVector3(data, "position");
    auto degrees = readVector3(data, "rotationDegrees");
    auto scale = readVector3(data, "scale");
    if (!position)
        return tl::unexpected(position.error());
    if (!degrees)
        return tl::unexpected(degrees.error());
    if (!scale)
        return tl::unexpected(scale.error());
    return world.teleport(entity, {*position, rotationDegrees(*degrees), *scale});
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
    return Json{{"verticalFovDegrees", glm::degrees(camera.verticalFov)},
                {"near", camera.nearPlane},
                {"far", camera.farPlane}};
}
Result<void> readCamera(World& world, entt::entity entity, const Json& data, DeserializeContext&) {
    auto fields = requireFields(data, {"verticalFovDegrees", "near", "far"});
    if (!fields)
        return fields;
    auto fov = readNumber(data, "verticalFovDegrees");
    auto nearPlane = readNumber(data, "near");
    auto farPlane = readNumber(data, "far");
    if (!fov)
        return tl::unexpected(fov.error());
    if (!nearPlane)
        return tl::unexpected(nearPlane.error());
    if (!farPlane)
        return tl::unexpected(farPlane.error());
    world.registry().emplace<Camera>(entity, glm::radians(*fov), *nearPlane, *farPlane);
    return {};
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
          ComponentCodec{"somnium.Transform", 1, contains<Transform>, writeTransform,
                         readTransform},
          ComponentCodec{"somnium.Hierarchy", 1, contains<Hierarchy>, writeHierarchy,
                         readHierarchy},
          ComponentCodec{"somnium.MeshRenderer", 1, contains<MeshRenderer>, writeMesh, readMesh},
          ComponentCodec{"somnium.Camera", 1, contains<Camera>, writeCamera, readCamera},
          ComponentCodec{"somnium.DirectionalLight", 1, contains<DirectionalLight>, writeLight,
                         readLight}}) {
        auto added = codecs.add(std::move(codec));
        if (!added)
            return added;
    }
    return {};
}
} // namespace somnium::scene
