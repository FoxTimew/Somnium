// Extracts interpolated scene poses without exposing renderer types to gameplay.
// Resource preparation and scratch reservation happen before frame recording.
#pragma once
#include "model_rendering.hpp"
#include <memory_resource>
#include <somnium/scene/world.hpp>

namespace somnium::runtime {
struct ModelDraws {
    assets::AssetHandle<assets::Model> handle;
    std::span<const renderer::Draw> draws;
};
struct SceneCapacity {
    std::size_t entities, draws;
};
struct SceneFrame {
    renderer::FrameData uniforms;
    std::pmr::vector<renderer::Draw> draws;
    std::pmr::vector<glm::mat4> worlds;
    std::array<float, 4> clearColor{};
    void reserve(SceneCapacity capacity);
};
[[nodiscard]] Result<void> extractScene(const scene::World& world, float alpha,
                                        gpu::FrameSize extent, std::span<const ModelDraws> models,
                                        SceneFrame& frame);
class SceneRendering final {
  public:
    SceneRendering(renderer::Renderer& renderer, assets::AssetManager<assets::Model>& models)
        : m_renderer(renderer), m_models(models) {}
    [[nodiscard]] Result<void> prepare(const scene::World& world);
    [[nodiscard]] Result<void> extract(const scene::World& world, float alpha,
                                       gpu::FrameSize size) {
        return extractScene(world, alpha, size, m_bindings, m_frame);
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
