// Keeps a stable Gameplay facade while exchanging generation-owned codecs and Worlds.
// Retired native images remain mapped; no retired callbacks remain in the active engine.
#pragma once
#include <somnium/runtime/game_module.hpp>

namespace somnium::runtime {
struct GameModuleConfig {
    std::string_view root;
    std::string_view publication = "game.json";
};
class SOMNIUM_ENGINE_TYPE ReloadableGame final : public Gameplay {
  public:
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<std::unique_ptr<ReloadableGame>>
    create(GameModuleConfig config = {});
    SOMNIUM_ENGINE_API ~ReloadableGame() override;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> start(scene::World&,
                                                        const StartContext&) override;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> fixedUpdate(scene::World&,
                                                              const FixedUpdate&) override;
    SOMNIUM_ENGINE_API void stop(scene::World&) override;
    [[nodiscard]] SOMNIUM_ENGINE_API const scene::CodecRegistry& codecs() const;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<bool> changed(double seconds);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void>
    replace(std::unique_ptr<scene::World>& world, const StartContext& context, bool playing,
            scene::AssetContext* assets,
            std::span<const scene::WorldSnapshot* const> preserved = {},
            std::string_view publication = {});
    [[nodiscard]] SOMNIUM_ENGINE_API bool usable() const;
    [[nodiscard]] SOMNIUM_ENGINE_API std::size_t imageCount() const;
    [[nodiscard]] SOMNIUM_ENGINE_API std::size_t retainedBytes() const;
    [[nodiscard]] SOMNIUM_ENGINE_API std::span<const ModuleStorage> storages() const;

  private:
    struct Impl;
    SOMNIUM_ENGINE_API explicit ReloadableGame(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::runtime
