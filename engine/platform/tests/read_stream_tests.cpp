// Exercises the exact FileReader loop with a deliberately fragmented SDL stream.
// No public API exposes SDL; the stream seam belongs to platform's private implementation.
#include "read_stream.hpp"

#include <SDL3/SDL_error.h>
#include <doctest/doctest.h>
#include <somnium/core/conventions.hpp>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string_view>

namespace {
struct Fragmented {
    std::string_view data = "fragmented bytes";
    Sint64 declaredSize = static_cast<Sint64>(data.size());
    std::size_t offset = 0;
    int calls = 0;
    SDL_IOStatus stop = SDL_IO_STATUS_READY;
};
Sint64 SDLCALL size(void* context) {
    return static_cast<Fragmented*>(context)->declaredSize;
}
// The two void* positions are imposed by SDL_IOStreamInterface's callback ABI.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::size_t SDLCALL read(void* context, void* destination, std::size_t requested,
                         SDL_IOStatus* status) {
    auto& source = *static_cast<Fragmented*>(context);
    ++source.calls;
    if (source.offset >= 3 && source.stop != SDL_IO_STATUS_READY) {
        *status = source.stop;
        if (source.stop == SDL_IO_STATUS_ERROR)
            SDL_SetError("injected read failure");
        return 0;
    }
    const auto count = std::min({requested, std::size_t{3}, source.data.size() - source.offset});
    std::memcpy(destination, source.data.data() + source.offset, count);
    source.offset += count;
    *status = count == 0 ? SDL_IO_STATUS_EOF : SDL_IO_STATUS_READY;
    return count;
}
struct Close {
    void operator()(SDL_IOStream* stream) const {
        SDL_CloseIO(stream);
    }
};
std::unique_ptr<SDL_IOStream, Close> open(Fragmented& source) {
    SDL_IOStreamInterface interface;
    SDL_INIT_INTERFACE(&interface);
    interface.size = size;
    interface.read = read;
    return std::unique_ptr<SDL_IOStream, Close>(SDL_OpenIO(&interface, &source));
}
} // namespace

TEST_CASE("FileReader's stream loop assembles all partial reads") {
    Fragmented source;
    auto stream = open(source);
    REQUIRE(stream);
    auto bytes = somnium::platform::detail::readStream(*stream);
    REQUIRE(bytes);
    CHECK(bytes->size() == source.data.size());
    CHECK(std::memcmp(bytes->data(), source.data.data(), bytes->size()) == 0);
    CHECK(source.calls == 6);
}

TEST_CASE("FileReader's stream loop terminates on early EOF, error or no progress") {
    for (const auto stop : {SDL_IO_STATUS_EOF, SDL_IO_STATUS_ERROR, SDL_IO_STATUS_NOT_READY}) {
        Fragmented source;
        source.stop = stop;
        auto stream = open(source);
        REQUIRE(stream);
        auto bytes = somnium::platform::detail::readStream(*stream);
        REQUIRE_FALSE(bytes);
        CHECK(bytes.error().code == somnium::ErrorCode::Io);
        CHECK_FALSE(bytes.error().message.empty());
        CHECK(source.calls == 2);
        if (stop == SDL_IO_STATUS_ERROR)
            CHECK(bytes.error().message == "injected read failure");
    }
}

TEST_CASE("FileReader handles empty streams and rejects oversized resources before allocating") {
    Fragmented source;
    source.declaredSize = 0;
    auto stream = open(source);
    REQUIRE(stream);
    auto empty = somnium::platform::detail::readStream(*stream);
    REQUIRE(empty);
    CHECK(empty->empty());
    CHECK(source.calls == 0);
    source.declaredSize = static_cast<Sint64>(somnium::core::kMaxResourceBytes) + 1;
    auto oversized = somnium::platform::detail::readStream(*stream);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error().code == somnium::ErrorCode::CapacityExceeded);
    CHECK(source.calls == 0);
}
