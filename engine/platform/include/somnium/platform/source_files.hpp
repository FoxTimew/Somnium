// Restricts development writes to an explicitly owned source root.
// Publication checks the last observed bytes and atomically replaces a sibling temporary.
#pragma once
#include <cstdint>
#include <somnium/core/export.hpp>
#include <somnium/platform/file_reader.hpp>
#include <utility>

namespace somnium::platform {
struct SourceUpdate {
    std::string_view expected;
    std::string_view contents;
};
class SourceFiles final {
  public:
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<SourceFiles> create(std::string_view root = {});
    [[nodiscard]] SOMNIUM_ENGINE_API Result<FileReader> dataReader() const;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<std::string> read(std::string_view logicalPath) const;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> replace(std::string_view logicalPath,
                                                          SourceUpdate update);

  private:
    explicit SourceFiles(std::string root) : m_root(std::move(root)) {}
    std::string m_root;
    std::uint64_t m_sequence = 0;
};
} // namespace somnium::platform
