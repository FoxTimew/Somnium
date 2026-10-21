// Keeps parsed shader bytes alive until the device has synchronously consumed them.
// Private to gpu and its CPU tests; no JSON type crosses the module boundary.
#pragma once
#include <somnium/gpu/device.hpp>

namespace somnium::gpu::detail {
struct ShaderData {
    core::Bytes bytes;
    ShaderStage stage;
    ShaderFormat format;
    std::string entrypoint;
    std::uint32_t samplers, storageTextures, storageBuffers, uniforms;
    [[nodiscard]] ShaderDescription description() const {
        return {stage,    format,          bytes,          entrypoint,
                samplers, storageTextures, storageBuffers, uniforms};
    }
};
[[nodiscard]] Result<ShaderData> readShader(const core::ByteReader& reader,
                                            std::string_view manifest, ShaderFormat format);
} // namespace somnium::gpu::detail
