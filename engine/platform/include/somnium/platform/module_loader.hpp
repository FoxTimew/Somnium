// Loads verified binary bytes from private copies, leaving linker outputs writable.
// Native images remain mapped until process exit because profiling can retain code metadata.
#pragma once
#include <memory>
#include <somnium/core/byte_reader.hpp>
#include <somnium/core/export.hpp>
#include <somnium/core/handle.hpp>
#include <span>

namespace somnium::platform {
using ModuleImage = Handle<struct ModuleImageTag>;
using ModuleFunction = void (*)();
struct ModuleBytes {
    std::string_view filename;
    std::span<const std::byte> binary;
    std::string_view symbolsFilename;
    std::span<const std::byte> symbols;
};
class SOMNIUM_ENGINE_TYPE ModuleLoader final : public core::ByteReader {
  public:
    // Empty resolves <executable>/modules. Explicit roots must be absolute.
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<std::unique_ptr<ModuleLoader>>
    create(std::string_view root = {});
    SOMNIUM_ENGINE_API ~ModuleLoader() override;
    ModuleLoader(const ModuleLoader&) = delete;
    ModuleLoader& operator=(const ModuleLoader&) = delete;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<core::Bytes> read(std::string_view path) const override;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<ModuleImage> load(ModuleBytes bytes);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<ModuleFunction> function(ModuleImage image,
                                                                     std::string_view name) const;
    [[nodiscard]] SOMNIUM_ENGINE_API std::size_t imageCount() const;
    // File bytes retained, not an estimate of resident memory.
    [[nodiscard]] SOMNIUM_ENGINE_API std::size_t retainedBytes() const;

  private:
    struct Impl;
    SOMNIUM_ENGINE_API explicit ModuleLoader(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::platform
