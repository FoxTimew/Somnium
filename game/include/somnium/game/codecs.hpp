// Registers the game's versioned components through the public scene codec table.
// The common checked readers and deferred entity resolver are the only decoding path.
#pragma once
#include <somnium/scene/serialization.hpp>
namespace somnium::game {
[[nodiscard]] Result<void> registerCodecs(scene::CodecRegistry& codecs);
} // namespace somnium::game
