// Resolves source paths independently of the process directory and refuses root escapes.
// The read roots keep compiled shaders first and source assets out of the build tree.
#include "source_files_atomic.hpp"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <array>
#include <somnium/core/logical_path.hpp>
#include <somnium/platform/source_files.hpp>

namespace somnium::platform {
namespace {
namespace fs = std::filesystem;
fs::path native(std::string_view text) {
    return fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}
std::string portable(const fs::path& path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
auto invalid(std::string message) {
    return tl::unexpected(Error{ErrorCode::InvalidArgument, std::move(message)});
}
Result<fs::path> resolve(const fs::path& directory, std::string_view logical) {
    auto canonical = core::logicalPath(logical);
    if (!canonical || *canonical != logical)
        return invalid("Non-canonical source path");
    std::error_code error;
    auto target = fs::weakly_canonical(directory / native(logical), error);
    if (error)
        return tl::unexpected(Error{ErrorCode::Io, error.message()});
    auto part = target.begin();
    for (const auto& expected : directory) {
        if (expected.empty())
            continue;
        if (part == target.end() || *part != expected)
            return invalid("Source path escapes its root");
        ++part;
    }
    if (part == target.end())
        return invalid("Source path is the root directory");
    return target;
}
} // namespace
Result<SourceFiles> SourceFiles::create(std::string_view root) {
    if (root.empty())
        root = SOMNIUM_DEVELOPMENT_ASSETS;
    if (!core::validUtf8(root) || root.find('\0') != std::string_view::npos ||
        !native(root).is_absolute())
        return invalid("Source root must be an absolute UTF-8 directory");
    std::error_code error;
    const auto directory = fs::canonical(native(root), error);
    if (error)
        return tl::unexpected(Error{ErrorCode::Io, error.message()});
    if (!fs::is_directory(directory, error) || error)
        return invalid("Source root is not a directory");
    return SourceFiles(portable(directory));
}
Result<FileReader> SourceFiles::dataReader() const {
    const auto* base = SDL_GetBasePath();
    if (!base)
        return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
    const std::string shaders = std::string(base) + "assets";
    const std::array<std::string_view, 3> roots{shaders, m_root, SOMNIUM_DEVELOPMENT_ASSETS};
    return FileReader::createOrdered(roots);
}
Result<std::string> SourceFiles::read(std::string_view logicalPath) const {
    auto target = resolve(native(m_root), logicalPath);
    if (!target)
        return tl::unexpected(target.error());
    auto reader = FileReader::create(m_root);
    if (!reader)
        return tl::unexpected(reader.error());
    auto bytes = reader->read(logicalPath);
    if (!bytes)
        return tl::unexpected(bytes.error());
    if (bytes->empty())
        return std::string{};
    return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}
Result<void> SourceFiles::replace(std::string_view logicalPath, SourceUpdate update) {
    auto target = resolve(native(m_root), logicalPath);
    if (!target)
        return tl::unexpected(target.error());
    const auto unchanged = [&]() -> Result<void> {
        auto actual = read(logicalPath);
        if (!actual)
            return tl::unexpected(actual.error());
        auto current = resolve(native(m_root), logicalPath);
        if (!current)
            return tl::unexpected(current.error());
        if (*actual != update.expected || *current != *target)
            return tl::unexpected(
                Error{ErrorCode::InvalidState, "Source changed externally; reload before saving"});
        return {};
    };
    auto checked = unchanged();
    if (!checked)
        return checked;
    if (update.contents == update.expected)
        return {};
    return detail::atomicReplace(*target, update.contents, m_sequence++, unchanged);
}
} // namespace somnium::platform
