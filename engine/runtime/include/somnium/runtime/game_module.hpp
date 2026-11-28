// Defines the local, toolchain-matched gameplay boundary for iteration builds.
// The primitive query checks the ABI before factories or C++ values cross that boundary.
#pragma once
#include <somnium/runtime/gameplay.hpp>
#include <somnium/scene/serialization.hpp>

namespace somnium::runtime {
struct ModuleStorage {
    std::string codec;
    entt::id_type id;
    std::size_t size, alignment;
};
struct ModuleRegistration {
    scene::CodecRegistry codecs;
    std::vector<ModuleStorage> storages;
    // Reconstructed subscriptions/context only. Called after stop, before World retirement.
    std::function<void(scene::World&)> detach;
};
struct GameModuleApi {
    Result<Gameplay*> (*create)(ModuleRegistration& registration) = nullptr;
    void (*destroy)(Gameplay*) = nullptr;
};
inline constexpr std::uint32_t kGameModuleVersion = 1;
using GameModuleQuery = bool (*)(std::uint32_t version, std::size_t apiSize,
                                 const char* fingerprint, GameModuleApi* output);
} // namespace somnium::runtime
