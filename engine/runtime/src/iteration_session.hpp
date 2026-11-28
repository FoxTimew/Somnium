// Exchanges complete Worlds at fixed-step boundaries using the scene snapshot contract.
// Failed starts/restores keep the previously active World and its asset ownership intact.
#pragma once
#include <somnium/core/export.hpp>
#include <somnium/runtime/gameplay.hpp>
#include <somnium/scene/serialization.hpp>

namespace somnium::runtime {
class IterationSession final {
  public:
    IterationSession(std::unique_ptr<scene::World>& world, Gameplay& game,
                     const scene::CodecRegistry& codecs, const input::Configuration& mapping,
                     scene::AssetContext* assets = nullptr)
        : m_world(world), m_game(game), m_codecs(codecs), m_mapping(mapping), m_assets(assets) {}
    SOMNIUM_ENGINE_API ~IterationSession();
    IterationSession(const IterationSession&) = delete;
    IterationSession& operator=(const IterationSession&) = delete;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> play(std::uint64_t tick);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> stop();
    [[nodiscard]] bool playing() const {
        return m_playing;
    }
    [[nodiscard]] const scene::WorldSnapshot& editingSnapshot() const {
        return m_editing;
    }
    void abandon() {
        m_playing = false;
    }

  private:
    std::unique_ptr<scene::World>& m_world;
    Gameplay& m_game;
    const scene::CodecRegistry& m_codecs;
    const input::Configuration& m_mapping;
    scene::AssetContext* m_assets;
    scene::WorldSnapshot m_editing;
    bool m_playing = false;
};
} // namespace somnium::runtime
