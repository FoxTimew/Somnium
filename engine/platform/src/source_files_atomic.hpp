// Publishes a fully flushed source file only after the caller rechecks its baseline.
// The private validation seam also exercises failures at the actual publication boundary.
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <somnium/core/export.hpp>
#include <somnium/core/result.hpp>
#include <string_view>

namespace somnium::platform::detail {
[[nodiscard]] SOMNIUM_ENGINE_API Result<void>
atomicReplace(const std::filesystem::path& target, std::string_view contents,
              std::uint64_t sequence, const std::function<Result<void>()>& validate);
}
