// Defines byte acquisition independently of desktop paths or storage APIs.
// Platform implementations are injected into consumers by runtime.
#pragma once

#include <somnium/core/result.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

namespace somnium::core {
using Bytes = std::vector<std::byte>;
class ByteReader {
  public:
    virtual ~ByteReader() = default;
    [[nodiscard]] virtual Result<Bytes> read(std::string_view logicalPath) const = 0;
};
} // namespace somnium::core
