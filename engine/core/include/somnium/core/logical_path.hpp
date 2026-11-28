// Canonicalizes portable resource identities without touching a filesystem.
// Parent segments may resolve inside a root, but cannot escape it.
#pragma once
#include <somnium/core/export.hpp>
#include <somnium/core/result.hpp>
#include <string_view>

namespace somnium::core {
[[nodiscard]] SOMNIUM_ENGINE_API bool validUtf8(std::string_view text);
[[nodiscard]] SOMNIUM_ENGINE_API Result<std::string> logicalPath(std::string_view path);
} // namespace somnium::core
