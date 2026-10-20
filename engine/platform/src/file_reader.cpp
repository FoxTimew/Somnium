// Reads bounded resources through SDL IO without assuming the working directory.
// Logical paths are relative slash-separated identifiers; OS roots stay private.
#include <somnium/platform/file_reader.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <somnium/core/conventions.hpp>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>

#include <memory>
#include <utility>

namespace somnium::platform {
namespace {
struct CloseStream {
    void operator()(SDL_IOStream* stream) const {
        if (!SDL_CloseIO(stream)) {
            Log::write("platform", Log::Level::Error, SDL_GetError());
        }
    }
};

bool validLogicalPath(std::string_view path) {
    if (path.empty() || path.front() == '/' || path.back() == '/' ||
        path.find_first_of("\\:") != std::string_view::npos ||
        path.find('\0') != std::string_view::npos) {
        return false;
    }
    while (!path.empty()) {
        const auto slash = path.find('/');
        const auto part = path.substr(0, slash);
        if (part.empty() || part == ".." || part == ".") {
            return false;
        }
        if (slash == std::string_view::npos) {
            break;
        }
        path.remove_prefix(slash + 1);
    }
    return true;
}
} // namespace

FileReader::FileReader(std::string root) : m_root(std::move(root)) {}

Result<FileReader> FileReader::create(std::string_view root) {
    if (root.find('\0') != std::string_view::npos) {
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Resource root contains NUL"});
    }
    std::string resolved(root);
    if (resolved.empty()) {
        const auto* base = SDL_GetBasePath();
        if (!base) {
            return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
        }
        resolved = base;
        resolved += "assets";
    }
    if (resolved.back() != '/' && resolved.back() != '\\') {
        resolved += '/';
    }
    return FileReader(std::move(resolved));
}

Result<core::Bytes> FileReader::read(std::string_view logicalPath) const {
    SOMNIUM_PROFILE_SCOPE("platform.read");
    if (!validLogicalPath(logicalPath)) {
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid logical resource path"});
    }
    const auto path = m_root + std::string(logicalPath);
    std::unique_ptr<SDL_IOStream, CloseStream> stream(SDL_IOFromFile(path.c_str(), "rb"));
    if (!stream) {
        return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
    }
    const auto length = SDL_GetIOSize(stream.get());
    if (length < 0) {
        return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
    }
    if (static_cast<std::uint64_t>(length) > core::kMaxResourceBytes) {
        return tl::unexpected(
            Error{ErrorCode::CapacityExceeded, "Resource exceeds the MVP read limit"});
    }
    core::Bytes bytes(static_cast<std::size_t>(length));
    if (!bytes.empty() && SDL_ReadIO(stream.get(), bytes.data(), bytes.size()) != bytes.size()) {
        return tl::unexpected(Error{ErrorCode::Io, "Resource read was incomplete: " + path});
    }
    if (!SDL_CloseIO(stream.release())) {
        return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
    }
    return bytes;
}
} // namespace somnium::platform
