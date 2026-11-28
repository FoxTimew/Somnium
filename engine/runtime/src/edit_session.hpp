// Coordinates source persistence and editor commands at the fixed-step boundary.
// The saved baseline is independent of the disposable simulation World.
#pragma once
#include "iteration_session.hpp"
#include <optional>
#include <somnium/core/export.hpp>
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
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> initialize();
    [[nodiscard]] SOMNIUM_ENGINE_API Result<EditEffect> execute(const scene::Json& command,
                                                                std::uint64_t tick);
    [[nodiscard]] bool dirty() const {
        return m_dirty;
    }
    [[nodiscard]] std::uint64_t revision() const {
        return m_revision;
    }
    [[nodiscard]] const std::string& error() const {
        return m_error;
    }
    [[nodiscard]] std::size_t undoCount() const {
        return m_cursor;
    }
    [[nodiscard]] std::size_t redoCount() const {
        return m_history.size() - m_cursor;
    }
    [[nodiscard]] SOMNIUM_ENGINE_API std::size_t historyBytes() const;
    [[nodiscard]] std::span<const scene::WorldSnapshot> history() const {
        return m_history;
    }
    void exchangedWorld() {
        ++m_revision;
    }

  private:
    [[nodiscard]] SOMNIUM_ENGINE_API Result<EditEffect> apply(const scene::Json& command,
                                                              std::uint64_t tick);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<EditEffect> restoreHistory(bool forward);
    SOMNIUM_ENGINE_API void remember(scene::WorldSnapshot before);
    SOMNIUM_ENGINE_API void trimHistory();
    std::unique_ptr<scene::World>& m_world;
    IterationSession& m_modes;
    const scene::CodecRegistry& m_codecs;
    platform::SourceFiles& m_files;
    std::string m_path;
    scene::AssetContext* m_assets;
    std::string m_source, m_saved, m_current, m_error;
    std::uint64_t m_revision = 0;
    bool m_dirty = false;
    // Entries before the cursor are undo states, entries after it are redo states.
    // Current World is not retained here; exchanges happen only after a valid restoration.
    std::vector<scene::WorldSnapshot> m_history;
    std::size_t m_cursor = 0, m_historyBytes = 0;
};
struct EditorReplayEntry {
    std::uint64_t step;
    scene::Json command;
    std::string errorContains;
};
[[nodiscard]] SOMNIUM_ENGINE_API Result<std::vector<EditorReplayEntry>>
parseEditorReplay(std::string_view text);
} // namespace somnium::runtime
