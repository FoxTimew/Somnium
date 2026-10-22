// Owns render resources independently of imported asset formats.
// Runtime supplies a draw list; the caller owns frame acquisition and submission.
#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <somnium/gpu/device.hpp>
#include <somnium/renderer/camera.hpp>

namespace somnium::renderer {
using Mesh = Handle<struct MeshTag>;
using Material = Handle<struct MaterialTag>;
using Texture = Handle<struct RenderTextureTag>;
struct MeshData {
    // Fixed position/normal/UV0 float layout: 32 bytes, offsets 0/12/24.
    std::span<const std::byte> vertices;
    std::span<const std::byte> indices;
    gpu::IndexFormat indexFormat;
};
struct MaterialData {
    Texture texture;
    std::array<float, 4> baseColor{1, 1, 1, 1};
    gpu::SamplerDescription sampler{};
};
struct FrameData {
    glm::mat4 view{1}, projection{1};
    glm::vec3 lightDirection{0, 0, 1};
    float ambient = 0;
};
struct Draw {
    Mesh mesh;
    Material material;
    glm::mat4 world{1};
};
using DrawList = std::vector<Draw>;
[[nodiscard]] Result<FrameData> cameraFrame(const Camera& camera, gpu::FrameSize extent);
class Renderer final {
  public:
    // Device and reader access stay on the creating thread; device must outlive this owner.
    [[nodiscard]] static Result<std::unique_ptr<Renderer>>
    create(gpu::Device& device, const core::ByteReader& reader,
           gpu::ColorFormat color = gpu::ColorFormat::Rgba8);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    [[nodiscard]] Result<Mesh> createMesh(const MeshData& data);
    [[nodiscard]] Result<Texture> createTexture(gpu::FrameSize size,
                                                std::span<const std::byte> rgba);
    [[nodiscard]] Result<Material> createMaterial(const MaterialData& data);
    [[nodiscard]] Result<void> destroy(Mesh handle);
    [[nodiscard]] Result<void> destroy(Material handle);
    [[nodiscard]] Result<void> destroy(Texture handle);
    // Allocate scratch capacity while loading, never implicitly during record().
    void reserveDraws(std::size_t count);
    [[nodiscard]] Result<void> record(const gpu::RenderPass& pass, const FrameData& frame,
                                      std::span<const Draw> draws);

  private:
    struct Impl;
    explicit Renderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::renderer
