// Private SDL implementation seam shared by FileReader and partial-read tests.
// A synchronous read must either make progress or report an IO error.
#pragma once

#include <SDL3/SDL_iostream.h>
#include <somnium/core/byte_reader.hpp>

namespace somnium::platform::detail {
[[nodiscard]] Result<core::Bytes> readStream(SDL_IOStream& stream);
}
