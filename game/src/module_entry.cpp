// Publishes a checked local ABI and generation-owned factories, never engine objects.
// Storage declarations let runtime retire every game pool before replacing its codecs.
#if defined(SOMNIUM_PROOF_ABI_MISMATCH)
#undef SOMNIUM_ENABLE_ITERATION
#endif
#include <cstring>
#include <somnium/game/codecs.hpp>
#include <somnium/game/sandbox_game.hpp>
#include <somnium/runtime/game_module.hpp>
#include <somnium/runtime/module_abi.hpp>
#if defined(SOMNIUM_MODULE_PROOF)
#include "../tests/module_fixture.hpp"
#endif

namespace {
template <class Type>
void storage(somnium::runtime::ModuleRegistration& registration, std::string name) {
    registration.storages.push_back(
        {std::move(name), entt::type_hash<Type>::value(), sizeof(Type), alignof(Type)});
}
somnium::Result<somnium::runtime::Gameplay*>
create(somnium::runtime::ModuleRegistration& registration) {
    auto registered = somnium::game::registerCodecs(registration.codecs);
    if (!registered)
        return tl::unexpected(registered.error());
    storage<somnium::game::AabbCollider>(registration, "game.AabbCollider");
    storage<somnium::game::Character>(registration, "game.Character");
    storage<somnium::game::Door>(registration, "game.Door");
    storage<somnium::game::FollowCamera>(registration, "game.FollowCamera");
#if defined(SOMNIUM_MODULE_PROOF)
    return somnium::game::proof::create(registration);
#else
    return new somnium::game::SandboxGame;
#endif
}
void destroy(somnium::runtime::Gameplay* game) {
    delete game;
}
} // namespace
#if defined(_WIN32)
#define SOMNIUM_GAME_EXPORT __declspec(dllexport)
#else
#define SOMNIUM_GAME_EXPORT __attribute__((visibility("default")))
#endif
extern "C" SOMNIUM_GAME_EXPORT bool somniumGameQuery(std::uint32_t version, std::size_t apiSize,
                                                     const char* fingerprint,
                                                     somnium::runtime::GameModuleApi* output) {
    if (version != somnium::runtime::kGameModuleVersion ||
        apiSize != sizeof(somnium::runtime::GameModuleApi) || !fingerprint || !output ||
        std::strcmp(fingerprint, somnium::runtime::compiledModuleAbi().c_str()) != 0)
        return false;
    *output = {create, destroy};
    return true;
}
