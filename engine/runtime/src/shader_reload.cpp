// Publishes a whole renderer program before frame recording and deduplicates diagnostics.
// Compiler failures and invalid publications are independent until each source recovers.
#include "shader_reload.hpp"
#include <nlohmann/json.hpp>
#include <somnium/core/log.hpp>
#include <somnium/core/sha256.hpp>

namespace somnium::runtime {
ShaderReload::ShaderReload(const core::ByteReader& reader, renderer::Renderer& renderer)
    : m_reader(reader), m_renderer(renderer), m_program(reader, "shaders/model.program.json"),
      m_status(reader, "shaders/model.status.json") {}
void ShaderReload::poll(double seconds) {
    bool changed = false;
    auto status = m_status.poll(seconds);
    if (!status) {
        m_compileError = status.error().message;
        Log::write("shader_reload", Log::Level::Warning, m_compileError);
        changed = true;
    } else if (const auto& publication = *status; publication.has_value()) {
        const auto& bytes = *publication;
        const auto* begin = reinterpret_cast<const char*>(bytes.data());
        const auto json = bytes.empty()
                              ? nlohmann::json{}
                              : nlohmann::json::parse(begin, begin + bytes.size(), nullptr, false);
        if (!json.is_object() || json.value("version", nlohmann::json{}) != 1 ||
            !json.contains("ok") || !json["ok"].is_boolean() || !json.contains("message") ||
            !json["message"].is_string())
            m_compileError = "Invalid shader watcher status";
        else
            m_compileError = json["ok"].get<bool>() ? "" : json["message"].get<std::string>();
        if (!m_compileError.empty())
            Log::write("shader_reload", Log::Level::Warning, m_compileError);
        changed = true;
    }
    auto program = m_program.poll(seconds);
    if (!program) {
        m_programError = program.error().message;
        Log::write("shader_reload", Log::Level::Warning, m_programError);
        changed = true;
    } else if (const auto& publication = *program; publication.has_value()) {
        auto result =
            m_renderer.reloadShaders(m_reader, "shaders/model.program.json", *publication);
        m_programError = result ? "" : result.error().message;
        changed = true;
        if (result && *result)
            Log::write("shader_reload", Log::Level::Info, "Activated program {}",
                       m_renderer.shaderGeneration());
        else if (!result)
            Log::write("shader_reload", Log::Level::Warning, "Publication {} rejected: {}",
                       core::sha256(*publication), m_programError);
    }
    if (!changed)
        return;
    auto diagnostic = m_compileError;
    if (!m_programError.empty()) {
        if (!diagnostic.empty())
            diagnostic += "\n";
        diagnostic += m_programError;
    }
    if (diagnostic != m_diagnostic) {
        m_diagnostic = std::move(diagnostic);
    }
}
} // namespace somnium::runtime
