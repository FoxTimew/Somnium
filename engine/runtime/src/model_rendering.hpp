// Adapts imported CPU model data to renderer resources and a flattened draw list.
// This private assembly layer is the only place that knows both assets and renderer.
#pragma once
#include <somnium/assets/model.hpp>
#include <somnium/core/export.hpp>
#include <somnium/renderer/renderer.hpp>

namespace somnium::runtime {
class ModelRendering final {
  public:
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<std::unique_ptr<ModelRendering>>
    create(renderer::Renderer& renderer, const assets::Model& model);
    SOMNIUM_ENGINE_API ~ModelRendering();
    ModelRendering(const ModelRendering&) = delete;
    ModelRendering& operator=(const ModelRendering&) = delete;
    [[nodiscard]] const renderer::DrawList& draws() const {
        return m_draws;
    }
    [[nodiscard]] std::span<const assets::PrimitiveIdentity> identities() const {
        return m_identities;
    }

  private:
    explicit ModelRendering(renderer::Renderer& renderer) : m_renderer(renderer) {}
    renderer::Renderer& m_renderer;
    std::vector<renderer::Texture> m_textures;
    std::vector<renderer::Material> m_materials;
    std::vector<renderer::Mesh> m_meshes;
    renderer::DrawList m_draws;
    std::vector<assets::PrimitiveIdentity> m_identities;
};
} // namespace somnium::runtime
