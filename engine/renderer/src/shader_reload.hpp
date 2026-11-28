// Validates and snapshots an immutable offline program before any GPU allocation.
// Reading a candidate from owned bytes avoids a second filesystem read during creation.
#pragma once
#include <array>
#include <map>
#include <somnium/core/byte_reader.hpp>
#include <somnium/core/export.hpp>
#include <span>

namespace somnium::renderer::detail {
struct ShaderProgram final : core::ByteReader {
    std::string generation, layout;
    std::array<std::string, 3> stages;
    std::map<std::string, core::Bytes, std::less<>> files;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<core::Bytes> read(std::string_view path) const override;
};
[[nodiscard]] SOMNIUM_ENGINE_API Result<ShaderProgram>
readProgram(const core::ByteReader& reader, std::string_view manifest,
            std::span<const std::byte> document);
} // namespace somnium::renderer::detail
