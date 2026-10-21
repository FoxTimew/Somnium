// Collects a bounded stream, including streams that return short successful reads.
// EOF, IO errors and zero progress before completion are explicit failures.
#include "read_stream.hpp"

#include <SDL3/SDL_error.h>
#include <somnium/core/conventions.hpp>

namespace somnium::platform::detail {
Result<core::Bytes> readStream(SDL_IOStream& stream) {
    const auto length = SDL_GetIOSize(&stream);
    if (length < 0) {
        return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
    }
    if (static_cast<std::uint64_t>(length) > core::kMaxResourceBytes) {
        return tl::unexpected(
            Error{ErrorCode::CapacityExceeded, "Resource exceeds the MVP read limit"});
    }
    core::Bytes bytes(static_cast<std::size_t>(length));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = SDL_ReadIO(&stream, bytes.data() + offset, bytes.size() - offset);
        if (SDL_GetIOStatus(&stream) == SDL_IO_STATUS_ERROR) {
            return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
        }
        if (count == 0) {
            return tl::unexpected(Error{ErrorCode::Io, "Resource read stopped before completion"});
        }
        offset += count;
    }
    return bytes;
}
} // namespace somnium::platform::detail
