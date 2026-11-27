// Interpolates local TRS first: child offsets follow their parent's rotation arc.
// The same hierarchy produces draw, camera and light poses at the presentation time.
#include "scene_rendering.hpp"
#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/quaternion.hpp>
#include <somnium/core/profile.hpp>

namespace somnium::runtime {
void SceneFrame::reserve(SceneCapacity capacity) {
    worlds.reserve(capacity.entities);
    draws.reserve(capacity.draws);
}
Result<void> extractScene(const scene::World& world, float alpha, gpu::FrameSize extent,
                          std::span<const ModelDraws> models, SceneFrame& frame,
                          const SceneView* view) {
    SOMNIUM_PROFILE_SCOPE("runtime.extract_scene");
    if (!std::isfinite(alpha) || alpha < 0 || alpha > 1 || !extent.drawable())
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid scene alpha or extent"});
    auto valid = world.validate();
    if (!valid)
        return valid;
    if (world.activeCamera == entt::null && !view)
        return tl::unexpected(Error{ErrorCode::InvalidState, "Scene has no active camera"});
    if (frame.worlds.capacity() < world.size())
        return tl::unexpected(Error{ErrorCode::InvalidState, "Scene entity scratch not reserved"});
    frame.worlds.clear();
    frame.draws.clear();
    frame.uniforms = {};
    frame.uniforms.ambient = world.settings.ambient;
    frame.clearColor = world.settings.clearColor;
    const auto& registry = world.registry();
    const auto traversal = world.traverse();
    for (auto entity : traversal) {
        const auto& transform = registry.get<Transform>(entity);
        auto rotation = transform.current.rotation;
        if (glm::dot(transform.previous.rotation, rotation) < 0)
            rotation = -rotation;
        const Trs local{glm::mix(transform.previous.position, transform.current.position, alpha),
                        glm::normalize(glm::slerp(transform.previous.rotation, rotation, alpha)),
                        glm::mix(transform.previous.scale, transform.current.scale, alpha)};
        const auto parent = registry.get<Hierarchy>(entity).parent;
        const auto pose =
            (parent == entt::null ? glm::mat4(1) : frame.worlds[world.orderIndex(parent)]) *
            scene::localMatrix(local);
        frame.worlds.push_back(pose);
        if (entity == world.activeCamera) {
            const auto& camera = registry.get<Camera>(entity);
            frame.uniforms.view = glm::inverse(pose);
            frame.uniforms.projection = glm::perspectiveRH_ZO(camera.verticalFov,
                                                              static_cast<float>(extent.width) /
                                                                  static_cast<float>(extent.height),
                                                              camera.nearPlane, camera.farPlane);
        }
        if (const auto* light = registry.try_get<DirectionalLight>(entity))
            frame.uniforms.lightDirection = glm::normalize(glm::mat3(pose) * light->direction);
        if (const auto* mesh = registry.try_get<MeshRenderer>(entity)) {
            const auto found = std::ranges::find(models, mesh->model, &ModelDraws::handle);
            if (found == models.end())
                return tl::unexpected(
                    Error{ErrorCode::InvalidHandle,
                          "Scene model has not been prepared: " + std::string(*world.id(entity))});
            if (found->draws.size() > frame.draws.capacity() - frame.draws.size())
                return tl::unexpected(
                    Error{ErrorCode::InvalidState, "Scene draw scratch not reserved"});
            for (const auto& draw : found->draws)
                frame.draws.push_back({draw.mesh, draw.material, pose * draw.world});
        }
    }
    if (view) {
        frame.uniforms.view = glm::inverse(scene::localMatrix(view->pose));
        frame.uniforms.projection = glm::perspectiveRH_ZO(
            view->lens.verticalFov,
            static_cast<float>(extent.width) / static_cast<float>(extent.height),
            view->lens.nearPlane, view->lens.farPlane);
    }
    return {};
}
Result<void> SceneRendering::prepare(const scene::World& world) {
    SOMNIUM_PROFILE_SCOPE("runtime.prepare_scene");
    for (auto& entry : m_cache)
        entry.used = false;
    std::size_t count = 0;
    const auto traversal = world.traverse();
    for (auto entity : traversal) {
        const auto* mesh = world.registry().try_get<MeshRenderer>(entity);
        if (!mesh)
            continue;
        auto found = std::ranges::find(m_cache, mesh->model, &Entry::handle);
        if (found == m_cache.end()) {
            const auto cpu = m_models.get(mesh->model);
            if (!cpu)
                return tl::unexpected(cpu.error());
            auto model = ModelRendering::create(m_renderer, cpu->get());
            if (!model)
                return tl::unexpected(model.error());
            m_cache.push_back({mesh->model, std::move(*model), true});
            found = std::prev(m_cache.end());
        }
        found->used = true;
        count += found->model->draws().size();
    }
    std::erase_if(m_cache, [](const Entry& entry) { return !entry.used; });
    m_bindings.clear();
    for (const auto& entry : m_cache)
        m_bindings.push_back({entry.handle, entry.model->draws()});
    m_frame.reserve({.entities = world.size(), .draws = count});
    m_renderer.reserveDraws(count);
    return {};
}
} // namespace somnium::runtime
