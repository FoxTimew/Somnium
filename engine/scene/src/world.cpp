// Maintains stable identities and intrusive links independently of recycled EnTT values.
// Cached traversal order changes only at structural boundaries, never during iteration.
#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <glm/ext/matrix_transform.hpp>
#include <limits>
#include <somnium/core/assert.hpp>
#include <somnium/core/profile.hpp>
#include <somnium/scene/world.hpp>

namespace somnium::scene {
namespace {
Error invalid(std::string message) {
    return {ErrorCode::InvalidArgument, std::move(message)};
}
bool finite(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
bool unit(const Trs& value) {
    return value.scale == glm::vec3(1);
}
} // namespace

glm::mat4 localMatrix(const Trs& local) {
    return glm::translate(glm::mat4(1), local.position) * glm::mat4_cast(local.rotation) *
           glm::scale(glm::mat4(1), local.scale);
}
Result<void> validateTrs(const Trs& local) {
    if (!finite(local.position) || !finite(local.scale) || local.scale.x == 0 ||
        local.scale.y == 0 || local.scale.z == 0 || !std::isfinite(local.rotation.w) ||
        !std::isfinite(local.rotation.x) || !std::isfinite(local.rotation.y) ||
        !std::isfinite(local.rotation.z) || std::abs(glm::length(local.rotation) - 1.F) > .0001F)
        return tl::unexpected(
            invalid("TRS must be finite, nonsingular and have a unit quaternion"));
    return {};
}
World::~World() {
    SOMNIUM_VERIFY(m_traversals == 0);
    for (auto handle : m_requests) {
        const auto released = m_assets->models.release(handle);
        SOMNIUM_VERIFY(released);
    }
}
World::Traversal::Traversal(const World& world) : m_world(world) {
    world.rebuildOrder();
    ++world.m_traversals;
    m_entities = world.m_order;
}
World::Traversal::~Traversal() {
    --m_world.m_traversals;
}
void World::reserve(std::size_t count) {
    SOMNIUM_VERIFY(m_traversals == 0);
    m_order.reserve(count);
    m_stack.reserve(count);
    m_orderIndices.reserve(count);
    m_requests.reserve(count);
}
Result<void> World::mutableStructure() const {
    SOMNIUM_ASSERT(m_traversals == 0);
    if (m_traversals != 0)
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Structural change during World traversal"});
    return {};
}
bool World::owns(entt::entity entity) const {
    if (!m_registry.valid(entity) || !m_registry.all_of<PersistentId, Transform, Hierarchy>(entity))
        return false;
    const auto& value = m_registry.get<PersistentId>(entity).value;
    const auto found = m_ids.find(value);
    return found != m_ids.end() && found->second == entity;
}
Result<entt::entity> World::find(std::string_view value) const {
    const auto found = m_ids.find(value);
    if (found == m_ids.end())
        return tl::unexpected(
            Error{ErrorCode::NotFound, "Unknown entity ID: " + std::string(value)});
    return found->second;
}
Result<std::string_view> World::id(entt::entity entity) const {
    if (!owns(entity))
        return tl::unexpected(Error{ErrorCode::InvalidHandle, "Entity does not belong to World"});
    return m_registry.get<PersistentId>(entity).value;
}
Result<void> World::advanceNextId(std::uint64_t next) {
    if (next == 0 || next < m_nextId)
        return tl::unexpected(invalid("nextId must be positive and cannot move backwards"));
    m_nextId = next;
    return {};
}
Result<entt::entity> World::create(std::string_view requested) {
    auto mutableResult = mutableStructure();
    if (!mutableResult)
        return tl::unexpected(mutableResult.error());
    std::string value(requested);
    if (value.empty()) {
        do {
            if (m_nextId == std::numeric_limits<std::uint64_t>::max())
                return tl::unexpected(
                    Error{ErrorCode::CapacityExceeded, "PersistentId counter exhausted"});
            value = std::format("runtime:{:016x}", m_nextId++);
        } while (m_ids.contains(value));
    } else {
        if (m_ids.contains(value))
            return tl::unexpected(invalid("Duplicate entity ID: " + value));
        if (value.starts_with("runtime:")) {
            std::uint64_t serial = 0;
            const auto* begin = value.data() + 8;
            const auto* end = value.data() + value.size();
            const auto parsed = std::from_chars(begin, end, serial, 16);
            if (value.size() != 24 || parsed.ec != std::errc{} || parsed.ptr != end || serial == 0)
                return tl::unexpected(invalid("Malformed reserved entity ID: " + value));
            if (serial == std::numeric_limits<std::uint64_t>::max())
                return tl::unexpected(
                    Error{ErrorCode::CapacityExceeded, "PersistentId counter exhausted"});
            m_nextId = std::max(m_nextId, serial + 1);
        }
    }
    const auto entity = m_registry.create();
    m_registry.emplace<PersistentId>(entity, value);
    m_registry.emplace<Name>(entity);
    m_registry.emplace<Transform>(entity);
    m_registry.emplace<Hierarchy>(entity);
    m_ids.emplace(std::move(value), entity);
    m_orderDirty = true;
    return entity;
}
void World::unlink(entt::entity entity) {
    auto& node = m_registry.get<Hierarchy>(entity);
    if (node.parent != entt::null) {
        auto& parent = m_registry.get<Hierarchy>(node.parent);
        if (parent.firstChild == entity)
            parent.firstChild = node.nextSibling;
        if (parent.lastChild == entity)
            parent.lastChild = node.previousSibling;
    }
    if (node.previousSibling != entt::null)
        m_registry.get<Hierarchy>(node.previousSibling).nextSibling = node.nextSibling;
    if (node.nextSibling != entt::null)
        m_registry.get<Hierarchy>(node.nextSibling).previousSibling = node.previousSibling;
    node.parent = node.previousSibling = node.nextSibling = entt::null;
}
Result<void> World::destroy(entt::entity entity) {
    auto mutableResult = mutableStructure();
    if (!mutableResult)
        return mutableResult;
    if (!owns(entity))
        return tl::unexpected(invalid("Cannot destroy an unknown entity"));
    unlink(entity);
    m_stack.clear();
    m_stack.push_back(entity);
    while (!m_stack.empty()) {
        const auto current = m_stack.back();
        m_stack.pop_back();
        for (auto child = m_registry.get<Hierarchy>(current).firstChild; child != entt::null;
             child = m_registry.get<Hierarchy>(child).nextSibling)
            m_stack.push_back(child);
        if (activeCamera == current)
            activeCamera = entt::null;
        m_ids.erase(m_registry.get<PersistentId>(current).value);
        (void)m_registry.destroy(current);
    }
    m_orderDirty = true;
    return {};
}
Result<void> World::unitScales(entt::entity edited, const Trs* local,
                               const entt::entity* parent) const {
    for (const auto& [name, entity] : m_ids) {
        if (!m_registry.any_of<Camera, DirectionalLight>(entity))
            continue;
        auto current = entity;
        std::size_t remaining = m_ids.size();
        while (current != entt::null) {
            if (remaining-- == 0 || !owns(current))
                return tl::unexpected(invalid(name + ": invalid ancestor chain"));
            const auto& transform = m_registry.get<Transform>(current);
            const auto& candidate = current == edited && local ? *local : transform.current;
            if (!unit(candidate) || !unit(transform.previous))
                return tl::unexpected(invalid(name +
                                              ": camera/light requires unit scale at ancestor " +
                                              m_registry.get<PersistentId>(current).value));
            current =
                current == edited && parent ? *parent : m_registry.get<Hierarchy>(current).parent;
        }
    }
    return {};
}
Result<void> World::reparent(entt::entity entity, entt::entity parent) {
    auto mutableResult = mutableStructure();
    if (!mutableResult)
        return mutableResult;
    if (!owns(entity) || (parent != entt::null && !owns(parent)))
        return tl::unexpected(invalid("Reparent requires entities from this World"));
    for (auto ancestor = parent; ancestor != entt::null;
         ancestor = m_registry.get<Hierarchy>(ancestor).parent)
        if (ancestor == entity)
            return tl::unexpected(
                invalid("Reparent would create a cycle at " + std::string(*id(entity))));
    auto scales = unitScales(entity, nullptr, &parent);
    if (!scales)
        return scales;
    unlink(entity);
    auto& node = m_registry.get<Hierarchy>(entity);
    node.parent = parent;
    if (parent != entt::null) {
        auto& target = m_registry.get<Hierarchy>(parent);
        node.previousSibling = target.lastChild;
        if (target.lastChild != entt::null)
            m_registry.get<Hierarchy>(target.lastChild).nextSibling = entity;
        else
            target.firstChild = entity;
        target.lastChild = entity;
    }
    m_orderDirty = true;
    synchronize(entity);
    return {};
}
void World::rebuildOrder() const {
    if (!m_orderDirty)
        return;
    SOMNIUM_VERIFY(m_traversals == 0);
    m_order.clear();
    m_stack.clear();
    m_orderIndices.clear();
    for (auto iterator = m_ids.rbegin(); iterator != m_ids.rend(); ++iterator)
        if (m_registry.get<Hierarchy>(iterator->second).parent == entt::null)
            m_stack.push_back(iterator->second);
    while (!m_stack.empty()) {
        const auto entity = m_stack.back();
        m_stack.pop_back();
        SOMNIUM_VERIFY(m_order.size() < m_ids.size());
        m_orderIndices.emplace(entity, m_order.size());
        m_order.push_back(entity);
        for (auto child = m_registry.get<Hierarchy>(entity).lastChild; child != entt::null;
             child = m_registry.get<Hierarchy>(child).previousSibling)
            m_stack.push_back(child);
    }
    SOMNIUM_VERIFY(m_order.size() == m_ids.size());
    m_orderDirty = false;
}
std::size_t World::orderIndex(entt::entity entity) const {
    rebuildOrder();
    const auto found = m_orderIndices.find(entity);
    SOMNIUM_VERIFY(found != m_orderIndices.end());
    return found->second;
}
void World::synchronize(entt::entity root) {
    const auto traversal = traverse();
    for (auto entity : traversal) {
        for (auto ancestor = entity; ancestor != entt::null;
             ancestor = m_registry.get<Hierarchy>(ancestor).parent) {
            if (ancestor == root) {
                auto& transform = m_registry.get<Transform>(entity);
                transform.previous = transform.current;
                break;
            }
        }
    }
}
Result<void> World::assignLocal(entt::entity entity, const Trs& local, bool teleporting) {
    if (!owns(entity))
        return tl::unexpected(invalid("Transform entity does not belong to World"));
    auto checked = validateTrs(local);
    if (!checked)
        return tl::unexpected(invalid(std::string(*id(entity)) + ": " + checked.error().message));
    auto scales = unitScales(entity, &local);
    if (!scales)
        return scales;
    auto& transform = m_registry.get<Transform>(entity);
    for (int axis = 0; axis < 3; ++axis)
        if (!teleporting &&
            std::signbit(local.scale[axis]) != std::signbit(transform.current.scale[axis]))
            return tl::unexpected(
                invalid(std::string(*id(entity)) + ": scale sign changes require teleport"));
    transform.current = local;
    if (teleporting)
        synchronize(entity);
    return {};
}
Result<void> World::setLocal(entt::entity entity, const Trs& local) {
    return assignLocal(entity, local, false);
}
Result<void> World::teleport(entt::entity entity, const Trs& local) {
    return assignLocal(entity, local, true);
}
void World::beginStep() {
    const auto traversal = traverse();
    for (auto entity : traversal) {
        auto& transform = m_registry.get<Transform>(entity);
        transform.previous = transform.current;
    }
}
Result<void> World::validate() const {
    if (!std::isfinite(settings.ambient) || settings.ambient < 0 || settings.ambient > 1)
        return tl::unexpected(invalid("settings.ambient must be in [0,1]"));
    for (float channel : settings.clearColor)
        if (!std::isfinite(channel) || channel < 0 || channel > 1)
            return tl::unexpected(invalid("settings.clearColor must be finite RGBA in [0,1]"));
    if (activeCamera != entt::null &&
        (!owns(activeCamera) || !m_registry.all_of<Camera>(activeCamera)))
        return tl::unexpected(invalid("activeCamera must reference a Camera entity"));
    unsigned lights = 0;
    for (const auto& [name, entity] : m_ids) {
        if (!owns(entity))
            return tl::unexpected(invalid(name + ": missing World components"));
        const auto& transform = m_registry.get<Transform>(entity);
        for (const Trs* local : {&transform.previous, &transform.current}) {
            auto checked = validateTrs(*local);
            if (!checked)
                return tl::unexpected(invalid(name + ": " + checked.error().message));
        }
        const auto parent = m_registry.get<Hierarchy>(entity).parent;
        if (parent != entt::null && !owns(parent))
            return tl::unexpected(invalid(name + ": missing parent"));
        if (const auto* camera = m_registry.try_get<Camera>(entity))
            if (!std::isfinite(camera->verticalFov) || camera->verticalFov <= 0 ||
                camera->verticalFov >= 3.14159265F || !std::isfinite(camera->nearPlane) ||
                !std::isfinite(camera->farPlane) || camera->nearPlane <= 0 ||
                camera->farPlane <= camera->nearPlane)
                return tl::unexpected(invalid(name + ": invalid Camera projection"));
        if (const auto* light = m_registry.try_get<DirectionalLight>(entity)) {
            ++lights;
            if (!finite(light->direction) || glm::dot(light->direction, light->direction) <= 0)
                return tl::unexpected(invalid(name + ": invalid light direction"));
        }
    }
    if (lights > 1)
        return tl::unexpected(invalid("Only one directional light is supported"));
    return unitScales();
}
Result<void> World::propagate() {
    SOMNIUM_PROFILE_SCOPE("scene.propagate");
    auto checked = validate();
    if (!checked)
        return checked;
    const auto traversal = traverse();
    for (auto entity : traversal) {
        auto& transform = m_registry.get<Transform>(entity);
        const auto parent = m_registry.get<Hierarchy>(entity).parent;
        transform.world =
            (parent == entt::null ? glm::mat4(1) : m_registry.get<Transform>(parent).world) *
            localMatrix(transform.current);
    }
    return {};
}
Result<assets::AssetHandle<assets::Model>> World::requestModel(std::string_view path) {
    if (!m_assets)
        return tl::unexpected(Error{ErrorCode::InvalidState, "World has no asset context"});
    auto handle = m_assets->models.acquire(path);
    if (!handle)
        return handle;
    const auto state = m_assets->models.state(*handle);
    if (!state || *state != assets::State::Ready) {
        Error error{ErrorCode::InvalidState, "Scene model is not Ready: " + std::string(path)};
        if (state && *state == assets::State::Failed) {
            const auto failure = m_assets->models.error(*handle);
            if (failure)
                error = {failure->code, std::string(path) + ": " + failure->message};
        }
        const auto released = m_assets->models.release(*handle);
        SOMNIUM_VERIFY(released);
        return tl::unexpected(std::move(error));
    }
    m_requests.push_back(*handle);
    return *handle;
}
Result<std::string_view> World::modelPath(assets::AssetHandle<assets::Model> handle) const {
    if (!m_assets)
        return tl::unexpected(Error{ErrorCode::InvalidState, "World has no asset context"});
    return m_assets->models.path(handle);
}
} // namespace somnium::scene
