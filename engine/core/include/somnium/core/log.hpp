// Exposes categories and levels without leaking spdlog or formatting types.
// Logging is the sole mutable global service permitted by the engine policy.
#pragma once

#include <string_view>

namespace somnium {
class Log final {
  public:
    enum class Level { Trace, Debug, Info, Warning, Error, Critical };
    static void write(std::string_view category, Level level, std::string_view message);
    static void flush();
};
} // namespace somnium
