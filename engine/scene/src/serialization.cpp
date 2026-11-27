// Loads into a temporary World and resolves every entity reference in a shared second pass.
// Validation uses checked JSON fields so malformed source data cannot trigger exceptions.
#include <algorithm>
#include <cmath>
#include <limits>
#include <somnium/core/profile.hpp>
#include <somnium/scene/serialization.hpp>

namespace somnium::scene {
namespace {
Error invalid(std::string message) {
    return {ErrorCode::InvalidArgument, std::move(message)};
}
Result<float> number(const Json& value, std::string_view field) {
    if (!value.is_number())
        return tl::unexpected(invalid(std::string(field) + ": number required"));
    const auto numeric = value.get<double>();
    if (!std::isfinite(numeric) || std::abs(numeric) > std::numeric_limits<float>::max())
        return tl::unexpected(invalid(std::string(field) + ": finite float required"));
    return static_cast<float>(numeric);
}
Error contextual(const World& world, entt::entity entity, std::string_view component,
                 const Error& error) {
    const auto identity = world.id(entity);
    return {error.code, "entity " + (identity ? std::string(*identity) : "<invalid>") + "/" +
                            std::string(component) + ": " + error.message};
}
} // namespace
Result<void> requireFields(const Json& object, std::initializer_list<std::string_view> fields) {
    if (!object.is_object())
        return tl::unexpected(invalid("Object required"));
    for (auto field : fields)
        if (!object.contains(std::string(field)))
            return tl::unexpected(invalid("Missing field: " + std::string(field)));
    for (const auto& item : object.items())
        if (std::find(fields.begin(), fields.end(), item.key()) == fields.end())
            return tl::unexpected(invalid("Unknown field: " + item.key()));
    return {};
}
Result<std::string> readString(const Json& object, std::string_view field) {
    if (!object.is_object())
        return tl::unexpected(invalid("Object required"));
    const auto found = object.find(std::string(field));
    if (found == object.end() || !found->is_string())
        return tl::unexpected(invalid(std::string(field) + ": string required"));
    return found->get<std::string>();
}
Result<float> readNumber(const Json& object, std::string_view field) {
    if (!object.is_object())
        return tl::unexpected(invalid("Object required"));
    const auto found = object.find(std::string(field));
    if (found == object.end())
        return tl::unexpected(invalid("Missing field: " + std::string(field)));
    return number(*found, field);
}
Result<glm::vec3> readVector3(const Json& object, std::string_view field) {
    if (!object.is_object())
        return tl::unexpected(invalid("Object required"));
    const auto found = object.find(std::string(field));
    if (found == object.end() || !found->is_array() || found->size() != 3)
        return tl::unexpected(invalid(std::string(field) + ": three numbers required"));
    glm::vec3 result;
    for (int axis = 0; axis < 3; ++axis) {
        auto item = number((*found)[static_cast<std::size_t>(axis)], field);
        if (!item)
            return tl::unexpected(item.error());
        result[axis] = *item;
    }
    return result;
}
void DeserializeContext::deferEntity(entt::entity owner, std::string field, std::string id,
                                     EntitySetter setter) {
    m_references.push_back({owner, std::move(field), std::move(id), std::move(setter)});
}
Result<void> DeserializeContext::resolveEntities() {
    for (const auto& reference : m_references) {
        auto target = m_world.find(reference.id);
        if (!target)
            return tl::unexpected(
                contextual(m_world, reference.owner, reference.field, target.error()));
        auto applied = reference.setter(m_world, reference.owner, *target);
        if (!applied)
            return tl::unexpected(
                contextual(m_world, reference.owner, reference.field, applied.error()));
    }
    m_references.clear();
    return {};
}
Result<void> CodecRegistry::add(ComponentCodec codec) {
    if (codec.name.empty() || codec.version == 0 || !codec.contains || !codec.serialize ||
        !codec.deserialize)
        return tl::unexpected(invalid("Codec requires a name, version and complete functions"));
    if (m_codecs.contains(codec.name))
        return tl::unexpected(invalid("Duplicate codec: " + codec.name));
    const auto name = codec.name;
    m_codecs.emplace(name, std::move(codec));
    return {};
}
const ComponentCodec* CodecRegistry::find(std::string_view name) const {
    const auto found = m_codecs.find(name);
    return found == m_codecs.end() ? nullptr : &found->second;
}
Result<std::string> serialize(const World& world, const CodecRegistry& codecs) {
    SOMNIUM_PROFILE_SCOPE("scene.serialize");
    auto valid = world.validate();
    if (!valid)
        return tl::unexpected(valid.error());
    Json output{{"version", 1},
                {"nextId", world.nextId()},
                {"settings",
                 {{"ambient", world.settings.ambient}, {"clearColor", world.settings.clearColor}}},
                {"activeCamera", nullptr},
                {"entities", Json::array()}};
    if (world.activeCamera != entt::null) {
        auto id = world.id(world.activeCamera);
        if (!id)
            return tl::unexpected(id.error());
        output["activeCamera"] = std::string(*id);
    }
    const auto traversal = world.traverse();
    std::vector<entt::entity> ordered(traversal.begin(), traversal.end());
    std::sort(ordered.begin(), ordered.end(),
              [&](auto left, auto right) { return *world.id(left) < *world.id(right); });
    for (auto entity : ordered) {
        Json components = Json::object();
        for (const auto& [name, codec] : codecs.codecs()) {
            if (!codec.contains(world, entity))
                continue;
            auto data = codec.serialize(world, entity);
            if (!data)
                return tl::unexpected(contextual(world, entity, name, data.error()));
            components[name] = {{"version", codec.version}, {"data", std::move(*data)}};
        }
        output["entities"].push_back(
            {{"id", std::string(*world.id(entity))}, {"components", std::move(components)}});
    }
    return output.dump(2) + "\n";
}
Result<std::unique_ptr<World>> deserialize(std::string_view text, const CodecRegistry& codecs,
                                           AssetContext* assets) {
    SOMNIUM_PROFILE_SCOPE("scene.deserialize");
    const auto data = Json::parse(text.begin(), text.end(), nullptr, false);
    auto fields =
        requireFields(data, {"version", "nextId", "settings", "activeCamera", "entities"});
    if (!fields)
        return tl::unexpected(fields.error());
    if (!data["version"].is_number_integer() || data["version"] != 1)
        return tl::unexpected(invalid("Unsupported scene version"));
    if (!data["nextId"].is_number_unsigned() || data["nextId"] == 0)
        return tl::unexpected(invalid("nextId must be a positive unsigned integer"));
    if (!data["entities"].is_array())
        return tl::unexpected(invalid("entities must be an array"));
    auto world = std::make_unique<World>(assets);
    auto next = world->advanceNextId(data["nextId"].get<std::uint64_t>());
    if (!next)
        return tl::unexpected(next.error());
    world->reserve(data["entities"].size());
    auto settings = requireFields(data["settings"], {"ambient", "clearColor"});
    if (!settings)
        return tl::unexpected(settings.error());
    auto ambient = readNumber(data["settings"], "ambient");
    if (!ambient)
        return tl::unexpected(ambient.error());
    world->settings.ambient = *ambient;
    const auto& clear = data["settings"]["clearColor"];
    if (!clear.is_array() || clear.size() != 4)
        return tl::unexpected(invalid("clearColor requires four numbers"));
    for (std::size_t index = 0; index < 4; ++index) {
        auto channel = number(clear[index], "clearColor");
        if (!channel)
            return tl::unexpected(channel.error());
        world->settings.clearColor[index] = *channel;
    }
    // All identities exist before codecs run; references still apply only in pass two.
    std::vector<entt::entity> entities;
    entities.reserve(data["entities"].size());
    for (const auto& record : data["entities"]) {
        auto shape = requireFields(record, {"id", "components"});
        if (!shape)
            return tl::unexpected(shape.error());
        auto id = readString(record, "id");
        if (!id)
            return tl::unexpected(id.error());
        if (id->empty())
            return tl::unexpected(invalid("Entity ID cannot be empty"));
        auto entity = world->create(*id);
        if (!entity)
            return tl::unexpected(entity.error());
        entities.push_back(*entity);
    }
    DeserializeContext context(*world);
    for (std::size_t index = 0; index < entities.size(); ++index) {
        const auto entity = entities[index];
        const auto& components = data["entities"][index]["components"];
        if (!components.is_object() || !components.contains("somnium.Transform"))
            return tl::unexpected(contextual(*world, entity, "components",
                                             invalid("Transform required in component object")));
        for (const auto& item : components.items()) {
            const auto* codec = codecs.find(item.key());
            if (!codec)
                return tl::unexpected(
                    contextual(*world, entity, item.key(), invalid("Unknown component")));
            auto shape = requireFields(item.value(), {"version", "data"});
            if (!shape)
                return tl::unexpected(contextual(*world, entity, item.key(), shape.error()));
            if (!item.value()["version"].is_number_integer() ||
                item.value()["version"] != codec->version)
                return tl::unexpected(contextual(*world, entity, item.key(),
                                                 invalid("Unsupported component version")));
            auto loaded = codec->deserialize(*world, entity, item.value()["data"], context);
            if (!loaded)
                return tl::unexpected(contextual(*world, entity, item.key(), loaded.error()));
        }
    }
    auto resolved = context.resolveEntities();
    if (!resolved)
        return tl::unexpected(resolved.error());
    if (!data["activeCamera"].is_null()) {
        auto camera = readString(data, "activeCamera");
        if (!camera)
            return tl::unexpected(camera.error());
        auto entity = world->find(*camera);
        if (!entity)
            return tl::unexpected(invalid("activeCamera: " + entity.error().message));
        world->activeCamera = *entity;
    }
    auto propagated = world->propagate();
    if (!propagated)
        return tl::unexpected(propagated.error());
    world->beginStep();
    return world;
}
Result<std::unique_ptr<World>> load(const core::ByteReader& reader, std::string_view path,
                                    const CodecRegistry& codecs, AssetContext* assets) {
    auto bytes = reader.read(path);
    if (!bytes)
        return tl::unexpected(bytes.error());
    const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    auto world = deserialize(text, codecs, assets);
    if (!world)
        return tl::unexpected(
            Error{world.error().code, std::string(path) + ": " + world.error().message});
    return world;
}
} // namespace somnium::scene
