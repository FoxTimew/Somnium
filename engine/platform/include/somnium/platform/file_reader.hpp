// Resolves logical resource names through ordered, explicitly owned roots.
// Callers see the ByteReader contract and never an OS file handle.
#pragma once
#include <somnium/core/export.hpp>

#include <somnium/core/byte_reader.hpp>

#include <span>
#include <string>

namespace somnium::platform {
class SOMNIUM_ENGINE_TYPE FileReader final : public core::ByteReader {
  public:
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<FileReader> create(std::string_view root = {});
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<FileReader>
    createOrdered(std::span<const std::string_view> roots);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<core::Bytes>
    read(std::string_view logicalPath) const override;

  private:
    SOMNIUM_ENGINE_API explicit FileReader(std::vector<std::string> roots);
    std::vector<std::string> m_roots;
};
} // namespace somnium::platform
