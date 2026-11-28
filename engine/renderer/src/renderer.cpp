// Translates renderer handles into indexed GPU draws with bounded frame scratch storage.
// Resource loading may allocate; record validates everything before emitting a render pass.
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <limits>
#include <somnium/core/profile.hpp>
#include <somnium/renderer/renderer.hpp>
#if defined(SOMNIUM_ENABLE_ITERATION)
#include "shader_reload.hpp"
#endif

namespace somnium::renderer {
namespace {
template <class Tag, class T> struct Store {
    HandlePool<Tag> pool;
    std::vector<std::unique_ptr<T>> values;
    Result<Handle<Tag>> insert(std::unique_ptr<T> value) {
        auto handle = pool.acquire();
        if (!handle)
            return tl::unexpected(handle.error());
        if (values.size() <= handle->index)
            values.resize(static_cast<std::size_t>(handle->index) + 1);
        values[handle->index] = std::move(value);
        return *handle;
    }
    T* get(Handle<Tag> handle) {
        return pool.contains(handle) ? values[handle.index].get() : nullptr;
    }
    Result<void> erase(Handle<Tag> handle) {
        auto result = pool.release(handle);
        if (result)
            values[handle.index].reset();
        return result;
    }
};
struct MeshResource {
    gpu::Device& device;
    gpu::Buffer vertices{}, indices{};
    std::uint32_t count = 0;
    explicit MeshResource(gpu::Device& owner) : device(owner) {}
    ~MeshResource() {
        if (indices)
            (void)device.destroy(indices);
        if (vertices)
            (void)device.destroy(vertices);
    }
};
struct TextureResource {
    gpu::Device& device;
    gpu::Texture texture{};
    std::size_t references = 0;
    explicit TextureResource(gpu::Device& owner) : device(owner) {}
    ~TextureResource() {
        if (texture)
            (void)device.destroy(texture);
    }
};
struct MaterialResource {
    gpu::Device& device;
    TextureResource& image;
    std::array<float, 4> factor;
    gpu::Sampler sampler{};
    MaterialResource(gpu::Device& owner, TextureResource& texture, std::array<float, 4> color)
        : device(owner), image(texture), factor(color) {
        ++image.references;
    }
    ~MaterialResource() {
        if (sampler)
            (void)device.destroy(sampler);
        --image.references;
    }
};
struct alignas(16) FrameUniform {
    glm::mat4 view, projection;
};
struct alignas(16) DrawUniform {
    glm::mat4 world, normal;
};
struct alignas(16) FragmentFrame {
    glm::vec4 lightAndEncode;
};
struct alignas(16) MaterialUniform {
    std::array<float, 4> factor;
};
static_assert(sizeof(FrameUniform) == 128 && sizeof(DrawUniform) == 128 &&
              sizeof(FragmentFrame) == 16 && sizeof(MaterialUniform) == 16);
struct Scratch {
    DrawUniform object;
    MaterialUniform material;
    std::array<gpu::TextureBinding, 1> textures;
    std::array<gpu::UniformData, 2> vertex;
    std::array<gpu::UniformData, 3> fragment;
};
template <class T> gpu::UniformData bytes(const T& value) {
    return std::as_bytes(std::span(&value, 1));
}
bool finite(const glm::mat4& matrix) {
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            if (!std::isfinite(matrix[column][row]))
                return false;
    return true;
}
Error invalid(std::string message) {
    return {ErrorCode::InvalidArgument, std::move(message)};
}
Result<void> createPipelines(gpu::Device& device, const core::ByteReader& reader,
                             const std::array<std::string_view, 3>& stages, gpu::ColorFormat color,
                             std::array<gpu::Pipeline, 4>& pipelines) {
    auto vertex = device.loadShader(reader, stages[0]);
    if (!vertex)
        return tl::unexpected(vertex.error());
    constexpr std::array attributes = {gpu::VertexAttribute{0, gpu::VertexFormat::Float3, 0},
                                       gpu::VertexAttribute{1, gpu::VertexFormat::Float3, 12},
                                       gpu::VertexAttribute{2, gpu::VertexFormat::Float2, 24}};
    for (std::size_t lighting = 0; lighting < 2; ++lighting) {
        auto fragment = device.loadShader(reader, stages[lighting + 1]);
        if (!fragment) {
            (void)device.destroy(*vertex);
            return tl::unexpected(fragment.error());
        }
        gpu::PipelineDescription description{*vertex, *fragment};
        description.color = color;
        description.vertexStride = 32;
        description.attributes = attributes;
        for (std::size_t winding = 0; winding < 2; ++winding) {
            description.frontFace =
                winding == 0 ? gpu::FrontFace::CounterClockwise : gpu::FrontFace::Clockwise;
            auto pipeline = device.createPipeline(description);
            if (!pipeline) {
                (void)device.destroy(*fragment);
                (void)device.destroy(*vertex);
                return tl::unexpected(pipeline.error());
            }
            pipelines[lighting * 2 + winding] = *pipeline;
        }
        (void)device.destroy(*fragment);
    }
    (void)device.destroy(*vertex);
    return {};
}
} // namespace

struct Renderer::Impl {
    gpu::Device& device;
    // Normal/mirrored winding, first Lambert then Lambert + ambient.
    std::array<gpu::Pipeline, 4> pipelines{};
    bool encode;
    gpu::ColorFormat color;
#if defined(SOMNIUM_ENABLE_ITERATION)
    std::string generation, layout;
#endif
    Store<RenderTextureTag, TextureResource> textures;
    Store<MaterialTag, MaterialResource> materials;
    Store<MeshTag, MeshResource> meshes;
    std::vector<Scratch> scratch;
    std::vector<gpu::Draw> draws;
    explicit Impl(gpu::Device& value, gpu::ColorFormat format)
        : device(value),
          encode(format == gpu::ColorFormat::Rgba8 || format == gpu::ColorFormat::Bgra8),
          color(format) {}
    ~Impl() {
        for (auto pipeline : pipelines)
            if (pipeline)
                (void)device.destroy(pipeline);
    }
};
Renderer::Renderer(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
Renderer::~Renderer() = default;

Result<std::unique_ptr<Renderer>>
Renderer::create(gpu::Device& device, const core::ByteReader& reader, gpu::ColorFormat color) {
    auto impl = std::make_unique<Impl>(device, color);
#if defined(SOMNIUM_ENABLE_ITERATION)
    auto document = reader.read("shaders/model.program.json");
    if (!document)
        return tl::unexpected(document.error());
    auto program = detail::readProgram(reader, "shaders/model.program.json", *document);
    if (!program)
        return tl::unexpected(program.error());
    const core::ByteReader& source = *program;
    const std::array<std::string_view, 3> stages = {program->stages[0], program->stages[1],
                                                    program->stages[2]};
    impl->generation = program->generation;
    impl->layout = program->layout;
#else
    const core::ByteReader& source = reader;
    constexpr std::array<std::string_view, 3> stages = {"shaders/modelVertex.vsMain.vert.json",
                                                        "shaders/modelFragment.psMain.frag.json",
                                                        "shaders/modelAmbient.psMain.frag.json"};
#endif
    auto created = createPipelines(device, source, stages, color, impl->pipelines);
    if (!created)
        return tl::unexpected(created.error());
    return std::unique_ptr<Renderer>(new Renderer(std::move(impl)));
}
#if defined(SOMNIUM_ENABLE_ITERATION)
Result<bool> Renderer::reloadShaders(const core::ByteReader& reader, std::string_view manifest,
                                     std::span<const std::byte> document) {
    auto candidate = detail::readProgram(reader, manifest, document);
    if (!candidate)
        return tl::unexpected(candidate.error());
    if (candidate->layout != m_impl->layout)
        return tl::unexpected(
            Error{ErrorCode::RestartRequired, "Shader layout changed: redémarrage requis"});
    if (candidate->generation == m_impl->generation)
        return false;
    // This temporary owns all candidates, then all retired pipelines after the swap.
    Impl resources(m_impl->device, m_impl->color);
    const std::array<std::string_view, 3> stages = {candidate->stages[0], candidate->stages[1],
                                                    candidate->stages[2]};
    auto created =
        createPipelines(m_impl->device, *candidate, stages, m_impl->color, resources.pipelines);
    if (!created)
        return tl::unexpected(created.error());
    m_impl->pipelines.swap(resources.pipelines);
    m_impl->generation = std::move(candidate->generation);
    return true;
}
std::string_view Renderer::shaderGeneration() const {
    return m_impl->generation;
}
#endif
Result<Mesh> Renderer::createMesh(const MeshData& data) {
    const std::size_t indexBytes = data.indexFormat == gpu::IndexFormat::Uint16 ? 2U : 4U;
    if (data.vertices.empty() || data.vertices.size() % 32 || data.indices.empty() ||
        data.indices.size() % (3U * indexBytes) || data.vertices.size() > UINT32_MAX ||
        data.indices.size() > UINT32_MAX)
        return tl::unexpected(invalid("Invalid packed mesh size"));
    auto resource = std::make_unique<MeshResource>(m_impl->device);
    auto vertex =
        m_impl->device.createVertexBuffer(static_cast<std::uint32_t>(data.vertices.size()));
    if (!vertex)
        return tl::unexpected(vertex.error());
    resource->vertices = *vertex;
    auto indices = m_impl->device.createIndexBuffer(static_cast<std::uint32_t>(data.indices.size()),
                                                    data.indexFormat);
    if (!indices)
        return tl::unexpected(indices.error());
    resource->indices = *indices;
    auto uploaded = m_impl->device.upload(*vertex, data.vertices);
    if (!uploaded)
        return tl::unexpected(uploaded.error());
    uploaded = m_impl->device.upload(*indices, data.indices);
    if (!uploaded)
        return tl::unexpected(uploaded.error());
    resource->count = static_cast<std::uint32_t>(data.indices.size() / indexBytes);
    return m_impl->meshes.insert(std::move(resource));
}
Result<Texture> Renderer::createTexture(gpu::FrameSize size, std::span<const std::byte> rgba) {
    auto resource = std::make_unique<TextureResource>(m_impl->device);
    auto texture =
        m_impl->device.createTexture({gpu::TextureKind::SampledRgba8Srgb, size.width, size.height});
    if (!texture)
        return tl::unexpected(texture.error());
    resource->texture = *texture;
    auto uploaded = m_impl->device.upload(*texture, rgba);
    if (!uploaded)
        return tl::unexpected(uploaded.error());
    return m_impl->textures.insert(std::move(resource));
}
Result<Material> Renderer::createMaterial(const MaterialData& data) {
    auto* texture = m_impl->textures.get(data.texture);
    if (!texture)
        return tl::unexpected(Error{ErrorCode::InvalidHandle, "Invalid material texture"});
    if (!std::all_of(data.baseColor.begin(), data.baseColor.end(),
                     [](float v) { return std::isfinite(v) && v >= 0 && v <= 1; }))
        return tl::unexpected(invalid("Invalid baseColor factor"));
    auto resource = std::make_unique<MaterialResource>(m_impl->device, *texture, data.baseColor);
    auto sampler = m_impl->device.createSampler(data.sampler);
    if (!sampler)
        return tl::unexpected(sampler.error());
    resource->sampler = *sampler;
    return m_impl->materials.insert(std::move(resource));
}
Result<void> Renderer::destroy(Mesh handle) {
    return m_impl->meshes.erase(handle);
}
Result<void> Renderer::destroy(Material handle) {
    return m_impl->materials.erase(handle);
}
Result<void> Renderer::destroy(Texture handle) {
    if (auto* texture = m_impl->textures.get(handle); texture && texture->references)
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Texture is referenced by a material"});
    return m_impl->textures.erase(handle);
}
void Renderer::reserveDraws(std::size_t count) {
    if (count > m_impl->scratch.size()) {
        m_impl->scratch.resize(count);
        m_impl->draws.resize(count);
    }
}
Result<void> Renderer::record(const gpu::RenderPass& pass, const FrameData& frame,
                              std::span<const Draw> draws) {
    SOMNIUM_PROFILE_SCOPE("renderer.record");
    if (draws.size() > m_impl->scratch.size())
        return tl::unexpected(
            Error{ErrorCode::CapacityExceeded, "Call reserveDraws while loading"});
    const float length = glm::length(frame.lightDirection);
    if (!finite(frame.view) || !finite(frame.projection) || !std::isfinite(length) ||
        length < 1e-6F || !std::isfinite(frame.ambient) || frame.ambient < 0 || frame.ambient > 1)
        return tl::unexpected(invalid("Invalid frame uniforms"));
    const FrameUniform camera{frame.view, frame.projection};
    const FragmentFrame lighting{
        glm::vec4(frame.lightDirection / length, m_impl->encode ? 1.0F : 0.0F)};
    const glm::vec4 ambient{frame.ambient, 0, 0, 0};
    const bool ambientEnabled = frame.ambient != 0;
    for (std::size_t i = 0; i < draws.size(); ++i) {
        const auto& source = draws[i];
        const auto* mesh = m_impl->meshes.get(source.mesh);
        const auto* material = m_impl->materials.get(source.material);
        if (!mesh || !material)
            return tl::unexpected(Error{ErrorCode::InvalidHandle, "Stale renderer draw handle"});
        const float determinant = glm::determinant(source.world);
        if (!finite(source.world) || !std::isfinite(determinant) || std::abs(determinant) < 1e-12F)
            return tl::unexpected(invalid("Noninvertible world matrix"));
        auto& scratch = m_impl->scratch[i];
        scratch.object = {source.world, glm::transpose(glm::inverse(source.world))};
        if (!finite(scratch.object.normal))
            return tl::unexpected(invalid("Invalid normal matrix"));
        scratch.material = {material->factor};
        scratch.textures = {gpu::TextureBinding{material->image.texture, material->sampler}};
        scratch.vertex = {bytes(camera), bytes(scratch.object)};
        scratch.fragment = {bytes(lighting), bytes(scratch.material), bytes(ambient)};
        m_impl->draws[i] = {
            .pipeline = m_impl->pipelines[(ambientEnabled ? 2U : 0U) + (determinant < 0 ? 1U : 0U)],
            .vertices = mesh->vertices,
            .textures = scratch.textures,
            .vertexUniforms = scratch.vertex,
            .fragmentUniforms = std::span(scratch.fragment).first(ambientEnabled ? 3U : 2U),
            .indices = mesh->indices,
            .indexCount = mesh->count};
    }
    return m_impl->device.recordPass(pass, std::span(m_impl->draws).first(draws.size()));
}
} // namespace somnium::renderer
