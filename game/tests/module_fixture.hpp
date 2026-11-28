// Exercises retirement of real game storage together with EnTT groups/context/signals.
// Failure variants still execute the real game and codecs before injecting one fault.
#pragma once
#include <entt/signal/sigh.hpp>
#include <somnium/core/assert.hpp>
#include <somnium/core/profile.hpp>

namespace somnium::game::SOMNIUM_GAME_GENERATION::proof {
struct Audit {
    entt::scoped_connection signal;
    unsigned updates = 0;
    void changed(entt::registry&, entt::entity) {
        ++updates;
    }
};
class Game final : public runtime::Gameplay {
  public:
    Result<void> start(scene::World& world, const runtime::StartContext& context) override {
        auto started = m_game.start(world, context);
        if (!started)
            return started;
        auto& registry = world.registry();
        SOMNIUM_VERIFY(!registry.ctx().contains<Audit>());
        auto& audit = registry.ctx().emplace<Audit>();
        audit.signal = registry.on_update<Transform>().connect<&Audit::changed>(audit);
        auto group = registry.group<Character>(entt::get<Transform>);
        SOMNIUM_VERIFY(group.size() == 1);
#if SOMNIUM_MODULE_PROOF == 2
        auto entity = world.find("character");
        if (!entity)
            return tl::unexpected(entity.error());
        auto pose = registry.get<Transform>(*entity).current;
        pose.position.x += 100;
        auto changed = world.teleport(*entity, pose);
        if (!changed)
            return changed;
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "proof: start rejected after mutation"});
#else
        return {};
#endif
    }
    Result<void> fixedUpdate(scene::World& world, const runtime::FixedUpdate& update) override {
        auto& registry = world.registry();
        SOMNIUM_VERIFY(registry.ctx().contains<Audit>());
        SOMNIUM_VERIFY(registry.group_if_exists<Character>(entt::get<Transform>).size() == 1);
        const auto entity = world.find("character");
        if (!entity)
            return tl::unexpected(entity.error());
        const auto before = registry.ctx().get<Audit>().updates;
        registry.patch<Transform>(*entity);
        SOMNIUM_VERIFY(registry.ctx().get<Audit>().updates == before + 1);
        return m_game.fixedUpdate(world, update);
    }
    void stop(scene::World& world) override {
        m_game.stop(world);
    }

  private:
    SandboxGame m_game;
};
inline Result<runtime::Gameplay*> create(runtime::ModuleRegistration& registration) {
    registration.detach = [](scene::World& world) { world.registry().ctx().erase<Audit>(); };
#if SOMNIUM_MODULE_PROOF == 3 || SOMNIUM_MODULE_PROOF == 4
    scene::CodecRegistry changed;
    for (const auto& [name, original] : registration.codecs.codecs()) {
        auto codec = original;
        if (name == "game.Character") {
            codec.deserialize = [](scene::World&, entt::entity, const scene::Json&,
                                   scene::DeserializeContext& context) -> Result<void> {
#if SOMNIUM_MODULE_PROOF == 4
                auto model = context.requestModel("models/missing-module-proof.glb");
                if (!model)
                    return tl::unexpected(model.error());
#else
                (void)context;
#endif
                return tl::unexpected(Error{ErrorCode::InvalidArgument, "proof: codec rejected"});
            };
        }
        auto added = changed.add(std::move(codec));
        if (!added)
            return tl::unexpected(added.error());
    }
    registration.codecs = std::move(changed);
#endif
    return new Game;
}
} // namespace somnium::game::SOMNIUM_GAME_GENERATION::proof
