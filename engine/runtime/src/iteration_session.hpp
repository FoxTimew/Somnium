// Exchanges complete Worlds at fixed-step boundaries using the scene snapshot contract.
// Failed starts/restores keep the previously active World and its asset ownership intact.
#pragma once
#include <somnium/runtime/gameplay.hpp>
#include <somnium/scene/serialization.hpp>

namespace somnium::runtime {
class IterationSession final {
  public:
    IterationSession(std::unique_ptr<scene::World>& world, Gameplay& game,
                     const scene::CodecRegistry& codecs, const input::Configuration& mapping,
                     scene::AssetContext* assets = nullptr)
        : m_world(world), m_game(game), m_codecs(codecs), m_mapping(mapping), m_assets(assets) {}
    ~IterationSession();
    [[nodiscard]] Result<void> play(std::uint64_t tick);
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool playing() const {
        return m_playing;
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
