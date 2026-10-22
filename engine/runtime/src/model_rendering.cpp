// Uploads each CPU primitive once, then references it from flattened node instances.
// Failed creation rolls back all already-created resources in dependency order.
#include "model_rendering.hpp"
#include <type_traits>

namespace somnium::runtime {
ModelRendering::~ModelRendering() {
    for (auto mesh : m_meshes)
        (void)m_renderer.destroy(mesh);
    for (auto material : m_materials)
        (void)m_renderer.destroy(material);
    for (auto texture : m_textures)
        (void)m_renderer.destroy(texture);
}
Result<std::unique_ptr<ModelRendering>> ModelRendering::create(renderer::Renderer& renderer,
                                                               const assets::Model& model) {
    auto result = std::unique_ptr<ModelRendering>(new ModelRendering(renderer));
    for (const auto& image : model.images) {
        auto texture = renderer.createTexture({image.width, image.height}, image.rgba);
        if (!texture)
            return tl::unexpected(texture.error());
        result->m_textures.push_back(*texture);
    }
    const auto filter = [](assets::Filter v) {
        return v == assets::Filter::Nearest ? gpu::Filter::Nearest : gpu::Filter::Linear;
    };
    const auto address = [](assets::AddressMode v) {
        return v == assets::AddressMode::Clamp ? gpu::AddressMode::Clamp : gpu::AddressMode::Repeat;
    };
    for (const auto& source : model.materials) {
        if (source.image >= result->m_textures.size())
            return tl::unexpected(
                Error{ErrorCode::InvalidArgument, "Model material image out of range"});
        auto material =
            renderer.createMaterial({result->m_textures[source.image],
                                     source.baseColor,
                                     {filter(source.sampler.min), filter(source.sampler.mag),
                                      address(source.sampler.u), address(source.sampler.v)}});
        if (!material)
            return tl::unexpected(material.error());
        result->m_materials.push_back(*material);
    }
    for (const auto& primitive : model.primitives) {
        if (primitive.material >= result->m_materials.size())
            return tl::unexpected(
                Error{ErrorCode::InvalidArgument, "Model primitive material out of range"});
        auto mesh = std::visit(
            [&](const auto& indices) {
                using Index = typename std::decay_t<decltype(indices)>::value_type;
                return renderer.createMesh(
                    {std::as_bytes(std::span(primitive.vertices)),
                     std::as_bytes(std::span(indices)),
                     sizeof(Index) == 2 ? gpu::IndexFormat::Uint16 : gpu::IndexFormat::Uint32});
            },
            primitive.indices);
        if (!mesh)
            return tl::unexpected(mesh.error());
        result->m_meshes.push_back(*mesh);
    }
    for (const auto& instance : model.instances) {
        if (instance.primitive >= model.primitives.size())
            return tl::unexpected(
                Error{ErrorCode::InvalidArgument, "Model instance primitive out of range"});
        const auto& primitive = model.primitives[instance.primitive];
        result->m_draws.push_back({result->m_meshes[instance.primitive],
                                   result->m_materials[primitive.material], instance.world});
        result->m_identities.push_back(primitive.identity);
    }
    renderer.reserveDraws(result->m_draws.size());
    return result;
}
} // namespace somnium::runtime
