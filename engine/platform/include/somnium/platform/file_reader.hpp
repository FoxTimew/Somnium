// Resolves logical resource names through ordered, explicitly owned roots.
// Callers see the ByteReader contract and never an OS file handle.
#pragma once

#include <somnium/core/byte_reader.hpp>

#include <span>
#include <string>

namespace somnium::platform {
class FileReader final : public core::ByteReader {
  public:
    [[nodiscard]] static Result<FileReader> create(std::string_view root = {});
    [[nodiscard]] static Result<FileReader> createOrdered(std::span<const std::string_view> roots);
    [[nodiscard]] Result<core::Bytes> read(std::string_view logicalPath) const override;

  private:
    explicit FileReader(std::vector<std::string> roots);
    std::vector<std::string> m_roots;
};
} // namespace somnium::platform
