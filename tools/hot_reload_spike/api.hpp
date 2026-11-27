// Describes the experiment's versioned entry point, never an installed engine API.
// C++ values require the same compiler, standard library, flags and engine image.
#pragma once
#include <somnium/runtime/gameplay.hpp>
#include <somnium/scene/serialization.hpp>

namespace somnium::spike {
inline constexpr std::uint32_t kAbi = 1;
struct ModuleApi {
    std::size_t stateSize;
    entt::id_type transformId, stateId;
    runtime::Gameplay* (*create)();
    void (*destroy)(runtime::Gameplay*);
    Result<void> (*registerCodecs)(scene::CodecRegistry&);
    bool (*logIsShared)();
    const void* (*profiler)();
};
using Query = bool (*)(std::uint32_t, std::size_t, ModuleApi*);
} // namespace somnium::spike
