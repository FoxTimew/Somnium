// Expresses recoverable failures without exceptions or borrowed diagnostics.
// Error codes are extended only when a concrete caller needs to distinguish them.
#pragma once

#include <cstdint>
#include <string>
#include <tl/expected.hpp>

namespace somnium {
enum class ErrorCode : std::uint8_t {
    InvalidArgument,
    InvalidHandle,
    CapacityExceeded,
    Platform,
    Gpu,
    Io,
    InvalidState,
    NotFound,
    RestartRequired
};

struct Error {
    ErrorCode code;
    std::string message;
};

template <typename T> using Result = tl::expected<T, Error>;
} // namespace somnium
