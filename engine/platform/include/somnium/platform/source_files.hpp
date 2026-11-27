// Restricts development writes to an explicitly owned source root.
// Publication checks the last observed bytes and atomically replaces a sibling temporary.
#pragma once
#include <cstdint>
#include <somnium/platform/file_reader.hpp>
#include <utility>

namespace somnium::platform {
struct SourceUpdate {
    std::string_view expected;
    std::string_view contents;
};
class SourceFiles final {
  public:
    [[nodiscard]] static Result<SourceFiles> create(std::string_view root = {});
    [[nodiscard]] Result<FileReader> dataReader() const;
    [[nodiscard]] Result<std::string> read(std::string_view logicalPath) const;
    [[nodiscard]] Result<void> replace(std::string_view logicalPath, SourceUpdate update);

  private:
    explicit SourceFiles(std::string root) : m_root(std::move(root)) {}
    std::string m_root;
    std::uint64_t m_sequence = 0;
};
} // namespace somnium::platform
