// Reads sources through ordered roots; only absence permits trying the next root.
// Canonical containment also prevents symlinks from escaping an injected root.
#include "read_stream.hpp"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <array>
#include <filesystem>
#include <memory>
#include <somnium/core/log.hpp>
#include <somnium/core/logical_path.hpp>
#include <somnium/core/profile.hpp>
#include <somnium/platform/file_reader.hpp>
#include <utility>

namespace somnium::platform {
namespace {
namespace fs = std::filesystem;
struct CloseStream {
    void operator()(SDL_IOStream* stream) const {
        if (!SDL_CloseIO(stream))
            Log::write("platform", Log::Level::Error, SDL_GetError());
    }
};
fs::path nativePath(std::string_view text) {
    return fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}
std::string portablePath(const fs::path& path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
Error ioError(const std::error_code& error) {
    return {ErrorCode::Io, error.message()};
}
bool within(const fs::path& path, std::string_view root) {
    auto candidate = path.begin();
    for (const auto& part : nativePath(root)) {
        if (part.empty())
            continue;
        if (candidate == path.end() || *candidate != part)
            return false;
        ++candidate;
    }
    return candidate != path.end();
}
} // namespace
FileReader::FileReader(std::vector<std::string> roots) : m_roots(std::move(roots)) {}
Result<FileReader> FileReader::createOrdered(std::span<const std::string_view> roots) {
    if (roots.empty())
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "No resource root"});
    std::vector<std::string> resolved;
    for (const auto root : roots) {
        if (!core::validUtf8(root) || root.empty() || root.find('\0') != std::string_view::npos ||
            !nativePath(root).is_absolute())
            return tl::unexpected(
                Error{ErrorCode::InvalidArgument, "Resource roots must be absolute"});
        // A later root may be absent on another machine; do not inspect it eagerly.
        resolved.push_back(portablePath(nativePath(root).lexically_normal()));
    }
    return FileReader(std::move(resolved));
}
Result<FileReader> FileReader::create(std::string_view root) {
    if (!root.empty())
        return createOrdered(std::span(&root, 1));
    const auto* base = SDL_GetBasePath();
    if (!base)
        return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
    const std::string installed = std::string(base) + "assets";
#if defined(SOMNIUM_DEVELOPMENT_ASSETS)
    const std::array<std::string_view, 2> roots{installed, SOMNIUM_DEVELOPMENT_ASSETS};
#else
    const std::array<std::string_view, 1> roots{installed};
#endif
    return createOrdered(roots);
}
Result<core::Bytes> FileReader::read(std::string_view logicalPath) const {
    SOMNIUM_PROFILE_SCOPE("platform.read");
    const auto normalized = core::logicalPath(logicalPath);
    if (!normalized || *normalized != logicalPath)
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Non-canonical logical resource path"});
    for (const auto& root : m_roots) {
        std::error_code error;
        const auto canonicalRoot = fs::weakly_canonical(nativePath(root), error);
        if (error == std::errc::no_such_file_or_directory)
            continue;
        if (error)
            return tl::unexpected(ioError(error));
        const auto candidate = fs::weakly_canonical(canonicalRoot / nativePath(logicalPath), error);
        if (error)
            return tl::unexpected(ioError(error));
        if (!within(candidate, portablePath(canonicalRoot)))
            return tl::unexpected(Error{ErrorCode::InvalidArgument, "Resource escapes its root"});
        const auto status = fs::status(candidate, error);
        if (error == std::errc::no_such_file_or_directory || (!error && !fs::exists(status)))
            continue;
        if (error)
            return tl::unexpected(ioError(error));
        if (!fs::is_regular_file(status))
            return tl::unexpected(Error{ErrorCode::Io, "Resource is not a regular file"});
        const auto path = portablePath(candidate);
        std::unique_ptr<SDL_IOStream, CloseStream> stream(SDL_IOFromFile(path.c_str(), "rb"));
        if (!stream)
            return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
        auto bytes = detail::readStream(*stream);
        if (!bytes)
            return bytes;
        if (!SDL_CloseIO(stream.release()))
            return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
        return bytes;
    }
    return tl::unexpected(
        Error{ErrorCode::NotFound, "Resource absent from all roots: " + std::string(logicalPath)});
}
} // namespace somnium::platform
