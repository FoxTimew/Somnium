// Exposes categories, levels and checked C++20 formatting without leaking spdlog.
// Logging is the sole mutable global service permitted by the engine policy.
#pragma once

#include <cstdint>
#include <format>
#include <string_view>
#include <utility>

namespace somnium {
class Log final {
  public:
    enum class Level : std::uint8_t { Trace, Debug, Info, Warning, Error, Critical, Off };
    // Default Info threshold; an exact category override replaces the default.
    static void setLevel(Level minimum);
    static void setCategoryLevel(std::string_view category, Level minimum);
    static void clearCategoryLevel(std::string_view category);
    [[nodiscard]] static bool enabled(std::string_view category, Level level);
    static void write(std::string_view category, Level level, std::string_view message);
    template <typename... Args>
        requires(sizeof...(Args) > 0)
    static void write(std::string_view category, Level level, std::format_string<Args...> pattern,
                      Args&&... arguments) {
        if (enabled(category, level)) {
            write(category, level, std::format(pattern, std::forward<Args>(arguments)...));
        }
    }
    static void flush();
};
} // namespace somnium
