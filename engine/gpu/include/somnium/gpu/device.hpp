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

enum class Backend : std::uint8_t { Vulkan, D3D12, Metal };
enum class ShaderFormat : std::uint8_t { Spirv, Dxil, Msl };
enum class ShaderStage : std::uint8_t { Vertex, Fragment };
enum class TextureKind : std::uint8_t {
    SampledRgba8,
    SampledRgba8Srgb,
    ColorRgba8,
    Depth32,
    ColorBgra8,
    ColorRgba8Srgb,
    ColorBgra8Srgb
};
enum class IndexFormat : std::uint8_t { Uint16, Uint32 };
enum class Filter : std::uint8_t { Nearest, Linear };
enum class AddressMode : std::uint8_t { Repeat, Clamp };
enum class Composition : std::uint8_t { Sdr, SdrLinear };
struct SamplerDescription {
    Filter min = Filter::Linear;
    Filter mag = Filter::Linear;
    AddressMode u = AddressMode::Repeat;
    AddressMode v = AddressMode::Repeat;
};
enum class ColorFormat : std::uint8_t { Rgba8, Bgra8, Rgba8Srgb, Bgra8Srgb };
enum class DepthFormat : std::uint8_t { None, Depth32 };
enum class CullMode : std::uint8_t { None, Front, Back };
enum class FrontFace : std::uint8_t { CounterClockwise, Clockwise };
enum class DepthCompare : std::uint8_t { Always, Less, LessEqual };
enum class VertexFormat : std::uint8_t { Float2, Float3, Float4 };

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
struct TextureBinding {
    Texture texture;
    Sampler sampler;
};
using UniformData = std::span<const std::byte>;
struct Draw {
    Pipeline pipeline;
    Buffer vertices;
    std::uint32_t firstVertex = 0;
    std::uint32_t vertexCount = 0;
    // Dense fragment slots, matching reflected shader counts. Borrowed until recordPass returns.
    std::span<const TextureBinding> textures{};
    std::span<const UniformData> vertexUniforms{};
    std::span<const UniformData> fragmentUniforms{};
    Buffer indices{};
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::int32_t baseVertex = 0;
};
struct RenderPass {
    // Empty color selects the frame's presentation target (normally the swapchain).
    // Empty depth disables depth.
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
    [[nodiscard]] Result<Buffer> createIndexBuffer(std::uint32_t bytes, IndexFormat format);
    [[nodiscard]] Result<Texture> createTexture(TextureDescription description);
    [[nodiscard]] Result<Sampler> createSampler(SamplerDescription description = {});
    [[nodiscard]] Result<Shader> createShader(const ShaderDescription& description);
    [[nodiscard]] Result<Shader> loadShader(const core::ByteReader& reader,
                                            std::string_view manifest);
    // Triangle lists, one color target, explicit vertex/depth/rasterizer state.
    [[nodiscard]] Result<Pipeline> createPipeline(const PipelineDescription& description);
    [[nodiscard]] Result<ColorFormat> attachWindow(platform::Platform& window,
                                                   Composition composition = Composition::Sdr);
    [[nodiscard]] Result<bool> supportsComposition(Composition composition) const;
    [[nodiscard]] Result<void> beginFrame();
    // Blocking VSYNC acquisition. A zero extent means no drawable (e.g. minimized).
    [[nodiscard]] Result<FrameSize> acquireSwapchain();
    // Tools may compose into an owned texture matching the acquired swapchain's extent/format.
    // Selection lasts for this frame and also applies to private cooperation render passes.
    [[nodiscard]] Result<void> setPresentationTarget(Texture texture);
    [[nodiscard]] Result<void> copyToSwapchain(Texture texture);
    [[nodiscard]] Result<void> recordPass(const RenderPass& pass, std::span<const Draw> draws);
    [[nodiscard]] Result<void> submitFrame();
    // Cancels offscreen recording; an acquired swapchain always forces submission.
    [[nodiscard]] Result<void> finishFrame();
    [[nodiscard]] Statistics statistics() const;
    [[nodiscard]] Result<void> upload(Buffer buffer, std::span<const std::byte> bytes);
    [[nodiscard]] Result<void> upload(Texture texture, std::span<const std::byte> rgba);
    // Convenience wrapper over beginFrame/recordPass/submitFrame, without presentation.
    [[nodiscard]] Result<void> renderOffscreen(const RenderPass& pass, std::span<const Draw> draws);
    // Explicit blocking readback, tightly packed RGBA8 (sRGB bytes remain encoded).
    // Tests/tools only, never an implicit per-frame wait.
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
