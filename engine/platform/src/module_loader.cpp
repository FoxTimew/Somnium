// Keeps filesystem conventions and SDL's native object handles inside platform.
// Only inert image handles and byte counts survive; callers own all queried callbacks.
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_loadso.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <somnium/core/logical_path.hpp>
#include <somnium/platform/file_reader.hpp>
#include <somnium/platform/module_loader.hpp>

namespace somnium::platform {
namespace {
namespace fs = std::filesystem;
fs::path native(std::string_view value) {
    return fs::path(
        std::u8string_view(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}
std::string portable(const fs::path& path) {
    const auto value = path.generic_u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
auto io(std::string message) {
    return tl::unexpected(Error{ErrorCode::Io, std::move(message)});
}
bool filename(std::string_view value) {
    auto checked = core::logicalPath(value);
    return checked && *checked == value && value.find('/') == std::string_view::npos &&
           value.find('\\') == std::string_view::npos && !value.empty();
}
Result<void> write(const fs::path& path, std::span<const std::byte> bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    stream.close();
    return stream ? Result<void>{}
                  : Result<void>{io("Could not write module copy: " + portable(path))};
}
} // namespace
struct ModuleLoader::Impl {
    FileReader source;
    fs::path shadow;
    std::vector<SDL_SharedObject*> images;
    std::size_t bytes = 0;
    std::uint64_t sequence = 0;
    Impl(FileReader reader, fs::path path) : source(std::move(reader)), shadow(std::move(path)) {}
};
ModuleLoader::ModuleLoader(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
ModuleLoader::~ModuleLoader() = default;
Result<std::unique_ptr<ModuleLoader>> ModuleLoader::create(std::string_view root) {
    std::string defaultRoot;
    if (root.empty()) {
        const char* base = SDL_GetBasePath();
        if (!base)
            return io(SDL_GetError());
        defaultRoot = std::string(base) + "modules";
        root = defaultRoot;
    }
    if (!core::validUtf8(root) || root.find('\0') != std::string_view::npos ||
        !native(root).is_absolute())
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Module root must be absolute UTF-8"});
    auto reader = FileReader::create(root);
    if (!reader)
        return tl::unexpected(reader.error());
    std::error_code error;
    auto directory = fs::canonical(native(root), error);
    if (error)
        return io(error.message());
    const auto copies = directory / ".loaded";
    fs::create_directories(copies, error);
    if (error)
        return io(error.message());
    const auto resolvedCopies = fs::canonical(copies, error);
    if (error || resolvedCopies != copies)
        return io("Module copy directory must not redirect outside its owned path");
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        const auto shadow = copies / (std::to_string(stamp) + "-" + std::to_string(attempt));
        if (fs::create_directory(shadow, error))
            return std::unique_ptr<ModuleLoader>(
                new ModuleLoader(std::make_unique<Impl>(std::move(*reader), shadow)));
        if (error)
            return io(error.message());
    }
    return io("Could not allocate a unique module-copy directory");
}
Result<core::Bytes> ModuleLoader::read(std::string_view path) const {
    return m_impl->source.read(path);
}
Result<ModuleImage> ModuleLoader::load(ModuleBytes bytes) {
    constexpr std::size_t kImageLimit = 64;
    if (m_impl->images.size() >= kImageLimit)
        return tl::unexpected(
            Error{ErrorCode::RestartRequired, "64 module images retained: redémarrage requis"});
    if (!filename(bytes.filename) || bytes.binary.empty() ||
        bytes.binary.size() > std::size_t{64} * 1024 * 1024 ||
        (bytes.symbols.empty() != bytes.symbolsFilename.empty()) ||
        (!bytes.symbols.empty() &&
         (!filename(bytes.symbolsFilename) || bytes.symbolsFilename == bytes.filename)) ||
        bytes.symbols.size() > std::size_t{256} * 1024 * 1024)
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid module files or size"});
    const auto folder = m_impl->shadow / std::to_string(m_impl->sequence++);
    std::error_code error;
    fs::create_directory(folder, error);
    if (error)
        return io(error.message());
    auto written = write(folder / native(bytes.filename), bytes.binary);
    if (!written)
        return tl::unexpected(written.error());
    if (!bytes.symbols.empty()) {
        written = write(folder / native(bytes.symbolsFilename), bytes.symbols);
        if (!written)
            return tl::unexpected(written.error());
    }
    const auto path = portable(folder / native(bytes.filename));
    auto* image = SDL_LoadObject(path.c_str());
    if (!image)
        return io(SDL_GetError());
    const auto index = static_cast<std::uint32_t>(m_impl->images.size());
    m_impl->images.push_back(image);
    m_impl->bytes += bytes.binary.size() + bytes.symbols.size();
    // Deliberately no SDL_UnloadObject, including images rejected by the caller's ABI query.
    // The pinned SDL implementations return OS handles, with no SDL allocation to abandon.
    return ModuleImage{index, 1};
}
Result<ModuleFunction> ModuleLoader::function(ModuleImage image, std::string_view name) const {
    if (image.generation != 1 || image.index >= m_impl->images.size() || name.empty() ||
        name.find('\0') != std::string_view::npos)
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid image or module symbol"});
    const std::string symbol(name);
    const auto address = SDL_LoadFunction(m_impl->images[image.index], symbol.c_str());
    if (!address)
        return io(SDL_GetError());
    return address;
}
std::size_t ModuleLoader::imageCount() const {
    return m_impl->images.size();
}
std::size_t ModuleLoader::retainedBytes() const {
    return m_impl->bytes;
}
} // namespace somnium::platform
