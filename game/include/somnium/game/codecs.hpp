// Registers the game's versioned components through the public scene codec table.
// The common checked readers and deferred entity resolver are the only decoding path.
#pragma once
#include <somnium/game/generation.hpp>
#include <somnium/scene/serialization.hpp>
namespace somnium::game::SOMNIUM_GAME_GENERATION {
[[nodiscard]] Result<void> registerCodecs(scene::CodecRegistry& codecs);
} // namespace somnium::game::SOMNIUM_GAME_GENERATION
