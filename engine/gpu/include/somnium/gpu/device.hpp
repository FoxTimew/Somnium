// Owns one graphics device and generation-checked resources on its creating thread.
// The platform video session must outlive the device. No native graphics type escapes.
#pragma once

#include <somnium/core/byte_reader.hpp>
#include <somnium/core/handle.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace somnium::gpu {
using Buffer = Handle<struct BufferTag>;
using Texture = Handle<struct TextureTag>;
using Shader = Handle<struct ShaderTag>;
using Sampler = Handle<struct SamplerTag>;
using Pipeline = Handle<struct PipelineTag>;

enum class Backend { Vulkan, D3D12, Metal };
enum class ShaderFormat { Spirv, Dxil, Msl };
enum class ShaderStage { Vertex, Fragment };
enum class TextureKind { SampledRgba8, ColorRgba8, Depth32 };

struct DeviceConfig {
    Backend backend;
    bool debug = false;
};
struct DeviceInfo {
    std::string driver;
    std::string name;
    std::string version;
    ShaderFormat shaderFormat;
};
struct ShaderDescription {
    ShaderStage stage;
    ShaderFormat format;
    std::span<const std::byte> code;
    std::string entrypoint;
    std::uint32_t samplers = 0;
    std::uint32_t storageTextures = 0;
    std::uint32_t storageBuffers = 0;
    std::uint32_t uniformBuffers = 0;
};
struct TextureDescription {
    TextureKind kind;
    std::uint32_t width;
    std::uint32_t height;
    // Optimized clear hints for render targets; actual clears belong to RenderPass.
    std::array<float, 4> clearColor = {0, 0, 0, 1};
    float clearDepth = 1.0F;
};
struct Vertex {
    std::array<float, 3> position;
    std::array<float, 4> color;
    std::array<float, 2> uv;
};
struct Draw {
    Pipeline pipeline;
    Buffer vertices;
    Texture texture;
    Sampler sampler;
    std::uint32_t firstVertex;
    std::uint32_t vertexCount;
};
struct RenderPass {
    Texture color;
    Texture depth;
    std::array<float, 4> clearColor;
    float clearDepth = 1.0F;
};

class Device final {
  public:
    [[nodiscard]] static Result<std::unique_ptr<Device>> create(DeviceConfig config);
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    [[nodiscard]] const DeviceInfo& info() const;
    [[nodiscard]] Result<Buffer> createVertexBuffer(std::uint32_t bytes);
    [[nodiscard]] Result<Texture> createTexture(TextureDescription description);
    [[nodiscard]] Result<Sampler> createNearestSampler();
    [[nodiscard]] Result<Shader> createShader(const ShaderDescription& description);
    [[nodiscard]] Result<Shader> loadShader(const core::ByteReader& reader,
                                            std::string_view manifest);
    // Vertex layout above, triangle list, RGBA8 + D32, CCW/BACK, LESS/write, no blending.
    // This pass contract accepts vertex shaders with no resources and one fragment sampler.
    [[nodiscard]] Result<Pipeline> createPipeline(Shader vertex, Shader fragment);
    [[nodiscard]] Result<void> upload(Buffer buffer, std::span<const std::byte> bytes);
    [[nodiscard]] Result<void> upload(Texture texture, std::span<const std::byte> rgba);
    // Records and submits before returning: no CPU command buffer can retain a handle.
    [[nodiscard]] Result<void> render(const RenderPass& pass, std::span<const Draw> draws);
    // Explicit blocking readback for tests/tools, never an implicit per-frame wait.
    [[nodiscard]] Result<std::vector<std::byte>> readback(Texture texture);
    [[nodiscard]] Result<void> destroy(Buffer handle);
    [[nodiscard]] Result<void> destroy(Texture handle);
    [[nodiscard]] Result<void> destroy(Shader handle);
    [[nodiscard]] Result<void> destroy(Sampler handle);
    [[nodiscard]] Result<void> destroy(Pipeline handle);

  private:
    struct Impl;
    explicit Device(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::gpu
