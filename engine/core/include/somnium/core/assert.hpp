// Separates debug-only programmer assertions from always-on verification.
// Expressions are evaluated once when enabled and never inside disabled assertions.
#pragma once
#include <somnium/core/export.hpp>

#include <source_location>

namespace somnium::core {
[[noreturn]] SOMNIUM_ENGINE_API void
assertionFailed(const char* expression,
                std::source_location location = std::source_location::current());
}

#define SOMNIUM_VERIFY(expression)                                                                 \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            ::somnium::core::assertionFailed(#expression);                                         \
        }                                                                                          \
    } while (false)

#ifndef NDEBUG
#define SOMNIUM_ASSERT(expression) SOMNIUM_VERIFY(expression)
#else
#define SOMNIUM_ASSERT(expression) ((void)0)
#endif
