// Proves indexed drawing, dense bindings, uniform slots and the sRGB transfer chain.
// Negative cases must fail before recording; both index widths render identical pixels.
#include "extensions.hpp"
#include <array>
#include <cmath>

namespace {
using namespace somnium;
using namespace somnium::gpu;
// NOLINTBEGIN(bugprone-macro-parentheses)
#define SOMNIUM_TEST_TRY(name, expression)                                                         \
    auto name = (expression);                                                                      \
    if (!name)                                                                                     \
    return tl::unexpected(name.error())
// NOLINTEND(bugprone-macro-parentheses)
Result<void> failed() {
    return tl::unexpected(Error{ErrorCode::InvalidState, "GPU extension contract failed"});
}
float decode(float value) {
    return value <= 0.04045F ? value / 12.92F : std::pow((value + 0.055F) / 1.055F, 2.4F);
}
int encode(float value) {
    return static_cast<int>(
        std::lround(255 * (value <= 0.0031308F ? 12.92F * value
                                               : 1.055F * std::pow(value, 1 / 2.4F) - 0.055F)));
}
} // namespace
somnium::Result<void> checkExtensions(somnium::gpu::Device& device,
                                      const somnium::core::ByteReader& shaders) {
    // Presentation readback must preserve encoding and normalize BGRA channel order.
    const std::array clear{0.25F, 0.5F, 0.75F, 1.F};
    for (const auto kind : {TextureKind::ColorRgba8, TextureKind::ColorBgra8,
                            TextureKind::ColorRgba8Srgb, TextureKind::ColorBgra8Srgb}) {
        SOMNIUM_TEST_TRY(color, device.createTexture({kind, 7, 3, clear}));
        if (device.setPresentationTarget(*color) || device.copyToSwapchain(*color))
            return failed(); // No acquired swapchain.
        SOMNIUM_TEST_TRY(cleared, device.renderOffscreen({*color, {}, clear}, {}));
        SOMNIUM_TEST_TRY(pixels, device.readback(*color));
        const bool srgb =
            kind == TextureKind::ColorRgba8Srgb || kind == TextureKind::ColorBgra8Srgb;
        for (std::size_t i = 0; i < pixels->size(); ++i) {
            const auto expected = srgb && i % 4 != 3
                                      ? encode(clear[i % 4])
                                      : static_cast<int>(std::lround(255 * clear[i % 4]));
            if (std::abs(std::to_integer<int>((*pixels)[i]) - expected) > 1)
                return failed();
        }
        SOMNIUM_TEST_TRY(released, device.destroy(*color));
    }
    SOMNIUM_TEST_TRY(vs, device.loadShader(shaders, "extensionsVertex.vsMain.vert.json"));
    SOMNIUM_TEST_TRY(ps, device.loadShader(shaders, "extensionsFragment.psMain.frag.json"));
    PipelineDescription description{*vs, *ps};
    description.depth = DepthFormat::None;
    description.depthTest = description.depthWrite = false;
    description.culling = CullMode::None;
    SOMNIUM_TEST_TRY(pipeline, device.createPipeline(description));
    SOMNIUM_TEST_TRY(target, device.createTexture({TextureKind::ColorRgba8, 8, 4}));
    SOMNIUM_TEST_TRY(first, device.createTexture({TextureKind::SampledRgba8Srgb, 1, 1}));
    SOMNIUM_TEST_TRY(second, device.createTexture({TextureKind::SampledRgba8Srgb, 1, 1}));
    const std::array<std::uint8_t, 4> firstPixel{128, 128, 128, 255}, secondPixel{64, 64, 64, 255};
    SOMNIUM_TEST_TRY(uploadFirst, device.upload(*first, std::as_bytes(std::span(firstPixel))));
    SOMNIUM_TEST_TRY(uploadSecond, device.upload(*second, std::as_bytes(std::span(secondPixel))));
    SOMNIUM_TEST_TRY(repeat, device.createSampler());
    SOMNIUM_TEST_TRY(clamp, device.createSampler({Filter::Nearest, Filter::Nearest,
                                                  AddressMode::Clamp, AddressMode::Clamp}));
    const std::array<Vertex, 4> vertices{{{{-1, -1, 0.5F}, {1, 1, 1, 1}, {0, 0}},
                                          {{1, -1, 0.5F}, {1, 1, 1, 1}, {2, 0}},
                                          {{1, 1, 0.5F}, {1, 1, 1, 1}, {2, 2}},
                                          {{-1, 1, 0.5F}, {1, 1, 1, 1}, {0, 2}}}};
    const auto vertexBytes = std::as_bytes(std::span(vertices));
    SOMNIUM_TEST_TRY(buffer,
                     device.createVertexBuffer(static_cast<std::uint32_t>(vertexBytes.size())));
    SOMNIUM_TEST_TRY(uploadVertices, device.upload(*buffer, vertexBytes));
    const std::array<std::uint16_t, 9> indices16{99, 99, 99, 1, 2, 3, 3, 4, 1};
    const std::array<std::uint32_t, 9> indices32{99, 99, 99, 1, 2, 3, 3, 4, 1};
    const std::array bindings{TextureBinding{*first, *repeat}, TextureBinding{*second, *clamp}};
    const std::array<float, 4> offset{0, 0, 0, 0}, weights{0.5F, 0.5F, 0, 0}, factor{0.5F, 1, 1, 1};
    const std::array<UniformData, 1> vertexUniforms{std::as_bytes(std::span(offset))};
    const std::array<UniformData, 2> fragmentUniforms{std::as_bytes(std::span(weights)),
                                                      std::as_bytes(std::span(factor))};
    const RenderPass pass{*target, {}, {0, 0, 0, 1}};
    for (const auto format : {IndexFormat::Uint16, IndexFormat::Uint32}) {
        const auto bytes = format == IndexFormat::Uint16
                               ? UniformData(std::as_bytes(std::span(indices16)))
                               : UniformData(std::as_bytes(std::span(indices32)));
        SOMNIUM_TEST_TRY(
            indices, device.createIndexBuffer(static_cast<std::uint32_t>(bytes.size()), format));
        Draw draw{.pipeline = *pipeline,
                  .vertices = *buffer,
                  .textures = bindings,
                  .vertexUniforms = vertexUniforms,
                  .fragmentUniforms = fragmentUniforms,
                  .indices = *indices,
                  .firstIndex = 3,
                  .indexCount = 6,
                  .baseVertex = -1};
        if (device.renderOffscreen(pass, std::span(&draw, 1)))
            return failed(); // Not uploaded.
        SOMNIUM_TEST_TRY(uploadIndices, device.upload(*indices, bytes));
        auto invalid = draw;
        invalid.indexCount = 9;
        if (device.renderOffscreen(pass, std::span(&invalid, 1)))
            return failed();
        invalid = draw;
        invalid.baseVertex = -2;
        if (device.renderOffscreen(pass, std::span(&invalid, 1)))
            return failed();
        invalid = draw;
        invalid.vertexUniforms = {};
        if (device.renderOffscreen(pass, std::span(&invalid, 1)))
            return failed();
        invalid = draw;
        invalid.textures = std::span(bindings).first(1);
        if (device.renderOffscreen(pass, std::span(&invalid, 1)))
            return failed();
        SOMNIUM_TEST_TRY(recording, device.beginFrame());
        SOMNIUM_TEST_TRY(rendered, device.recordPass(pass, std::span(&draw, 1)));
        SOMNIUM_TEST_TRY(retired, device.destroy(*indices));
        const auto stale = device.recordPass(pass, std::span(&draw, 1));
        if (stale || stale.error().code != ErrorCode::InvalidHandle ||
            device.statistics().pendingReleases != 1)
            return failed();
        SOMNIUM_TEST_TRY(submitted, device.submitFrame());
        SOMNIUM_TEST_TRY(pixels, device.readback(*target));
        const auto light = (decode(128 / 255.0F) + decode(64 / 255.0F)) * 0.5F;
        const std::array expected{encode(light * 0.5F), encode(light), encode(light), 255};
        for (std::size_t i = 0; i < pixels->size(); ++i) {
            if (std::abs(std::to_integer<int>((*pixels)[i]) - expected[i % 4]) > 1)
                return failed();
        }
    }
    return {};
}
