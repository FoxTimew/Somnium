// Reuses scene transactions for arbitrary engine and game component edits.
// Unchanged interpolation histories survive; changed transform branches are teleported.
#include "scene_editor.hpp"

namespace somnium::runtime {
namespace {
using namespace scene;
auto invalid(std::string message) {
    return tl::unexpected(Error{ErrorCode::InvalidArgument, std::move(message)});
}
Json* record(Json& document, std::string_view id) {
    for (auto& entity : document["entities"])
        if (entity["id"].get_ref<const std::string&>() == id)
            return &entity;
    return nullptr;
}
std::map<std::string, const Json*, std::less<>> indexed(const Json& document) {
    std::map<std::string, const Json*, std::less<>> records;
    for (const auto& entity : document["entities"])
        records.emplace(entity["id"].get<std::string>(), &entity["components"]);
    return records;
}
Result<std::unique_ptr<World>> preserveHistory(const WorldSnapshot& before, World& candidate,
                                               const CodecRegistry& codecs, AssetContext* assets) {
    auto saved = snapshot(candidate, codecs);
    if (!saved)
        return tl::unexpected(saved.error());
    const auto oldDocument = Json::parse(before.canonical);
    const auto newDocument = Json::parse(saved->canonical);
    const auto oldRecords = indexed(oldDocument), newRecords = indexed(newDocument);
    const auto oldHistory = Json::parse(before.temporal);
    auto history = Json::parse(saved->temporal);
    const auto unchanged = [&](std::string_view id, std::string_view name) {
        const auto old = oldRecords.find(id), next = newRecords.find(id);
        return old != oldRecords.end() && next != newRecords.end() && old->second->contains(name) &&
               next->second->contains(name) && (*old->second)[name] == (*next->second)[name];
    };
    for (auto& entityData : history.items()) {
        for (auto& component : entityData.value().items()) {
            bool keep = unchanged(entityData.key(), component.key());
            if (keep && component.key() == "somnium.Transform") {
                auto entity = *candidate.find(entityData.key());
                while (entity != entt::null && keep) {
                    const auto id = *candidate.id(entity);
                    keep = unchanged(id, "somnium.Transform") && unchanged(id, "somnium.Hierarchy");
                    entity = candidate.registry().get<Hierarchy>(entity).parent;
                }
            }
            if (keep && oldHistory.contains(entityData.key()) &&
                oldHistory[entityData.key()].contains(component.key()))
                component.value() = oldHistory[entityData.key()][component.key()];
        }
    }
    saved->temporal = history.dump();
    return restore(*saved, codecs, assets);
}
} // namespace
Result<EditedWorld> editWorld(const scene::World& source, const scene::CodecRegistry& codecs,
                              const scene::Json& command, scene::AssetContext* assets) {
    using namespace scene;
    auto operation = readString(command, "op");
    if (!operation)
        return tl::unexpected(operation.error());
    auto before = snapshot(source, codecs);
    if (!before)
        return tl::unexpected(before.error());
    std::string selected;
    if (*operation == "create" || *operation == "delete" || *operation == "reparent") {
        auto fields = *operation == "create"
                          ? requireFields(command, {"op", "id", "name", "parent"})
                      : *operation == "delete" ? requireFields(command, {"op", "id"})
                                               : requireFields(command, {"op", "id", "parent"});
        if (!fields)
            return tl::unexpected(fields.error());
        auto id = readString(command, "id");
        if (!id)
            return tl::unexpected(id.error());
        auto candidate = restore(*before, codecs, assets);
        if (!candidate)
            return tl::unexpected(candidate.error());
        auto& world = **candidate;
        auto entity = *operation == "create" ? world.create(*id) : world.find(*id);
        if (!entity)
            return tl::unexpected(entity.error());
        selected = std::string(*world.id(*entity));
        if (*operation == "delete") {
            auto deleted = world.destroy(*entity);
            if (!deleted)
                return tl::unexpected(deleted.error());
            selected.clear();
        } else {
            auto parent = readString(command, "parent");
            if (!parent)
                return tl::unexpected(parent.error());
            Result<entt::entity> parentEntity = entt::null;
            if (!parent->empty())
                parentEntity = world.find(*parent);
            if (!parentEntity)
                return tl::unexpected(parentEntity.error());
            auto linked = world.reparent(*entity, *parentEntity);
            if (!linked)
                return tl::unexpected(linked.error());
            if (*operation == "create") {
                auto name = readString(command, "name");
                if (!name)
                    return tl::unexpected(name.error());
                world.registry().get<Name>(*entity).value = std::move(*name);
            }
        }
        // Rebuild ownership from the surviving logical references, including game codecs.
        auto saved = snapshot(world, codecs);
        if (!saved)
            return tl::unexpected(saved.error());
        auto final = restore(*saved, codecs, assets);
        if (!final)
            return tl::unexpected(final.error());
        return EditedWorld{std::move(*final), std::move(selected)};
    }
    auto document = Json::parse(before->canonical);
    if (*operation == "settings") {
        auto fields = requireFields(command, {"op", "data"});
        if (!fields)
            return tl::unexpected(fields.error());
        fields = requireFields(command["data"], {"settings", "activeCamera"});
        if (!fields)
            return tl::unexpected(fields.error());
        document["settings"] = command["data"]["settings"];
        document["activeCamera"] = command["data"]["activeCamera"];
    } else {
        if (*operation != "set" && *operation != "add" && *operation != "remove")
            return invalid("Unknown editor operation: " + *operation);
        auto fields = *operation == "remove"
                          ? requireFields(command, {"op", "id", "component"})
                          : requireFields(command, {"op", "id", "component", "data"});
        if (!fields)
            return tl::unexpected(fields.error());
        auto id = readString(command, "id"), name = readString(command, "component");
        if (!id)
            return tl::unexpected(id.error());
        if (!name)
            return tl::unexpected(name.error());
        const auto* codec = codecs.find(*name);
        if (!codec)
            return invalid("Unknown component: " + *name);
        auto* entity = record(document, *id);
        if (!entity)
            return invalid("Unknown editor entity: " + *id);
        auto& components = (*entity)["components"];
        if ((*operation == "add") == components.contains(*name))
            return invalid("Component presence disagrees with editor operation: " + *name);
        if (*operation == "remove") {
            if (*name == "somnium.Transform" || *name == "somnium.Name" ||
                *name == "somnium.Hierarchy")
                return invalid("Required structural component cannot be removed");
            components.erase(*name);
        } else {
            components[*name] = {{"version", codec->version}, {"data", command["data"]}};
        }
        selected = std::move(*id);
    }
    auto candidate = deserialize(document.dump(), codecs, assets);
    if (!candidate)
        return tl::unexpected(candidate.error());
    auto final = preserveHistory(*before, **candidate, codecs, assets);
    if (!final)
        return tl::unexpected(final.error());
    return EditedWorld{std::move(*final), std::move(selected)};
}
} // namespace somnium::runtime
