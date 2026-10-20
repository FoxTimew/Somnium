// Resolves logical resource names relative to an explicitly owned root.
// Callers see the ByteReader contract and never an OS file handle.
#pragma once

#include <somnium/core/byte_reader.hpp>

#include <string>

namespace somnium::platform {
class FileReader final : public core::ByteReader {
  public:
    [[nodiscard]] static Result<FileReader> create(std::string_view root = {});
    [[nodiscard]] Result<core::Bytes> read(std::string_view logicalPath) const override;

  private:
    explicit FileReader(std::string root);
    std::string m_root;
};
} // namespace somnium::platform
