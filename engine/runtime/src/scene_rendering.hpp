// Extracts interpolated scene poses without exposing renderer types to gameplay.
// Resource preparation and scratch reservation happen before frame recording.
#pragma once
#include "model_rendering.hpp"
#include <memory_resource>
#include <somnium/core/export.hpp>
#include <somnium/scene/world.hpp>

namespace somnium::runtime {
struct ModelDraws {
    assets::AssetHandle<assets::Model> handle;
    std::span<const renderer::Draw> draws;
};
struct SceneCapacity {
    std::size_t entities, draws;
};
struct SceneView {
    Trs pose;
    Camera lens;
};
struct SceneFrame {
    renderer::FrameData uniforms;
    std::pmr::vector<renderer::Draw> draws;
    std::pmr::vector<glm::mat4> worlds;
    std::array<float, 4> clearColor{};
    SOMNIUM_ENGINE_API void reserve(SceneCapacity capacity);
};
[[nodiscard]] SOMNIUM_ENGINE_API Result<void> extractScene(const scene::World& world, float alpha,
                                                           gpu::FrameSize extent,
                                                           std::span<const ModelDraws> models,
                                                           SceneFrame& frame,
                                                           const SceneView* view = nullptr);
class SceneRendering final {
  public:
    SceneRendering(renderer::Renderer& renderer, assets::AssetManager<assets::Model>& models)
        : m_renderer(renderer), m_models(models) {}
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> prepare(const scene::World& world);
    [[nodiscard]] Result<void> extract(const scene::World& world, float alpha, gpu::FrameSize size,
                                       const SceneView* view = nullptr) {
        return extractScene(world, alpha, size, m_bindings, m_frame, view);
    }
    [[nodiscard]] const SceneFrame& frame() const {
        return m_frame;
    }

  private:
    struct Entry {
        assets::AssetHandle<assets::Model> handle;
        std::unique_ptr<ModelRendering> model;
        bool used = false;
    };
    renderer::Renderer& m_renderer;
    assets::AssetManager<assets::Model>& m_models;
    std::vector<Entry> m_cache;
    std::vector<ModelDraws> m_bindings;
    SceneFrame m_frame;
};
} // namespace somnium::runtime
