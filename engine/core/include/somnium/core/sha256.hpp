// Identifies published offline artifacts by their bytes, independently of file metadata.
// The returned lowercase digest is compatible with the offline Python publisher.
#pragma once
#include <cstddef>
#include <somnium/core/export.hpp>
#include <span>
#include <string>

namespace somnium::core {
[[nodiscard]] SOMNIUM_ENGINE_API std::string sha256(std::span<const std::byte> bytes);
}
