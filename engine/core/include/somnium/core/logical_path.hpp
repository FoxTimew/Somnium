// Canonicalizes portable resource identities without touching a filesystem.
// Parent segments may resolve inside a root, but cannot escape it.
#pragma once
#include <somnium/core/result.hpp>
#include <string_view>

namespace somnium::core {
[[nodiscard]] bool validUtf8(std::string_view text);
[[nodiscard]] Result<std::string> logicalPath(std::string_view path);
} // namespace somnium::core
