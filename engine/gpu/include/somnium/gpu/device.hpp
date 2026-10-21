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
#include <string_view>
#include <vector>

namespace somnium::platform {
class Platform;
}

namespace somnium::gpu {
namespace detail {
struct Access;
}
using Buffer = Handle<struct BufferTag>;
using Texture = Handle<struct TextureTag>;
using Shader = Handle<struct ShaderTag>;
using Sampler = Handle<struct SamplerTag>;
using Pipeline = Handle<struct PipelineTag>;

enum class Backend { Vulkan, D3D12, Metal };
enum class ShaderFormat { Spirv, Dxil, Msl };
enum class ShaderStage { Vertex, Fragment };
enum class TextureKind { SampledRgba8, ColorRgba8, Depth32 };
enum class ColorFormat { Rgba8, Bgra8, Rgba8Srgb, Bgra8Srgb };
enum class DepthFormat { None, Depth32 };
enum class CullMode { None, Front, Back };
enum class FrontFace { CounterClockwise, Clockwise };
enum class DepthCompare { Always, Less, LessEqual };
enum class VertexFormat { Float2, Float3, Float4 };

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
struct VertexAttribute {
    std::uint32_t location;
    VertexFormat format;
    std::uint32_t offset;
};
inline constexpr std::array kVertexAttributes = {
    VertexAttribute{0, VertexFormat::Float3, offsetof(Vertex, position)},
    VertexAttribute{1, VertexFormat::Float4, offsetof(Vertex, color)},
    VertexAttribute{2, VertexFormat::Float2, offsetof(Vertex, uv)}};
struct PipelineDescription {
    Shader vertex;
    Shader fragment;
    ColorFormat color = ColorFormat::Rgba8;
    DepthFormat depth = DepthFormat::Depth32;
    std::uint32_t vertexStride = sizeof(Vertex);
    std::span<const VertexAttribute> attributes = kVertexAttributes;
    CullMode culling = CullMode::Back;
    FrontFace frontFace = FrontFace::CounterClockwise;
    DepthCompare depthCompare = DepthCompare::Less;
    bool depthTest = true;
    bool depthWrite = true;
};
struct FrameSize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    [[nodiscard]] bool drawable() const {
        return width != 0 && height != 0;
    }
};
struct Statistics {
    std::uint64_t drawCalls = 0;
    std::uint64_t submissions = 0;
    std::uint64_t uploads = 0;
    std::uint64_t pendingReleases = 0;
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
    // An empty color handle selects the acquired swapchain; empty depth disables depth.
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
    // Triangle lists, one color target, explicit vertex/depth/rasterizer state.
    [[nodiscard]] Result<Pipeline> createPipeline(const PipelineDescription& description);
    [[nodiscard]] Result<ColorFormat> attachWindow(platform::Platform& window);
    [[nodiscard]] Result<void> beginFrame();
    // Blocking VSYNC acquisition. A zero extent means no drawable (e.g. minimized).
    [[nodiscard]] Result<FrameSize> acquireSwapchain();
    [[nodiscard]] Result<void> recordPass(const RenderPass& pass, std::span<const Draw> draws);
    [[nodiscard]] Result<void> submitFrame();
    // Cancels offscreen recording; an acquired swapchain always forces submission.
    [[nodiscard]] Result<void> finishFrame();
    [[nodiscard]] Statistics statistics() const;
    [[nodiscard]] Result<void> upload(Buffer buffer, std::span<const std::byte> bytes);
    [[nodiscard]] Result<void> upload(Texture texture, std::span<const std::byte> rgba);
    // Convenience wrapper over beginFrame/recordPass/submitFrame, without presentation.
    [[nodiscard]] Result<void> renderOffscreen(const RenderPass& pass, std::span<const Draw> draws);
    // Explicit blocking readback for tests/tools, never an implicit per-frame wait.
    [[nodiscard]] Result<std::vector<std::byte>> readback(Texture texture);
    [[nodiscard]] Result<void> destroy(Buffer handle);
    [[nodiscard]] Result<void> destroy(Texture handle);
    [[nodiscard]] Result<void> destroy(Shader handle);
    [[nodiscard]] Result<void> destroy(Sampler handle);
    [[nodiscard]] Result<void> destroy(Pipeline handle);

  private:
    friend struct detail::Access;
    struct Impl;
    explicit Device(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::gpu
