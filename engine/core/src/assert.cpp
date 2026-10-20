// Reports violated invariants with source location and terminates consistently.
// A bounded stack buffer keeps the assertion path independent of engine allocation.
#include <somnium/core/assert.hpp>
#include <somnium/core/log.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>

namespace somnium::core {
[[noreturn]] void assertionFailed(const char* expression, std::source_location location) {
    std::array<char, 2048> message{};
    std::snprintf(message.data(), message.size(), "%s:%u: %s: %s", location.file_name(),
                  static_cast<unsigned>(location.line()), location.function_name(), expression);
    Log::write("assert", Log::Level::Critical, message.data());
    Log::flush();
    std::abort();
}
} // namespace somnium::core
