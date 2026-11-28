// Coordinates optional publication polling at fixed-step boundaries.
// Compilation belongs to the external watcher; failures only update a read-only diagnostic.
#pragma once
#include <somnium/core/export.hpp>
#include <somnium/platform/file_watch.hpp>
#include <somnium/renderer/renderer.hpp>

namespace somnium::runtime {
class ShaderReload final {
  public:
    SOMNIUM_ENGINE_API ShaderReload(const core::ByteReader& reader, renderer::Renderer& renderer);
    SOMNIUM_ENGINE_API void poll(double seconds);
    [[nodiscard]] std::string_view diagnostic() const {
        return m_diagnostic;
    }

  private:
    const core::ByteReader& m_reader;
    renderer::Renderer& m_renderer;
    platform::FileWatch m_program, m_status;
    std::string m_compileError, m_programError, m_diagnostic;
};
} // namespace somnium::runtime
