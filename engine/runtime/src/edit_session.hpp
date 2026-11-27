// Coordinates source persistence and editor commands at the fixed-step boundary.
// The saved baseline is independent of the disposable simulation World.
#pragma once
#include "iteration_session.hpp"
#include <optional>
#include <somnium/platform/source_files.hpp>

namespace somnium::runtime {
struct EditEffect {
    std::optional<std::string> selected;
    std::string component;
};
class EditSession final {
  public:
    EditSession(std::unique_ptr<scene::World>& world, IterationSession& modes,
                const scene::CodecRegistry& codecs, platform::SourceFiles& files, std::string path,
                scene::AssetContext* assets = nullptr)
        : m_world(world), m_modes(modes), m_codecs(codecs), m_files(files), m_path(std::move(path)),
          m_assets(assets) {}
    [[nodiscard]] Result<void> initialize();
    [[nodiscard]] Result<EditEffect> execute(const scene::Json& command, std::uint64_t tick);
    [[nodiscard]] bool dirty() const {
        return m_dirty;
    }
    [[nodiscard]] std::uint64_t revision() const {
        return m_revision;
    }
    [[nodiscard]] const std::string& error() const {
        return m_error;
    }

  private:
    [[nodiscard]] Result<EditEffect> apply(const scene::Json& command, std::uint64_t tick);
    std::unique_ptr<scene::World>& m_world;
    IterationSession& m_modes;
    const scene::CodecRegistry& m_codecs;
    platform::SourceFiles& m_files;
    std::string m_path;
    scene::AssetContext* m_assets;
    std::string m_source, m_saved, m_current, m_error;
    std::uint64_t m_revision = 0;
    bool m_dirty = false;
};
struct EditorReplayEntry {
    std::uint64_t step;
    scene::Json command;
    std::string errorContains;
};
[[nodiscard]] Result<std::vector<EditorReplayEntry>> parseEditorReplay(std::string_view text);
} // namespace somnium::runtime
