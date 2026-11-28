// Applies an explicit editor command to a candidate World through its ordinary codecs.
// Persistent IDs and owned snapshots keep no references into the active registry.
#pragma once
#include <somnium/scene/serialization.hpp>
namespace somnium::runtime {
struct EditedWorld {
    std::unique_ptr<scene::World> world;
    std::string selected;
    scene::WorldSnapshot before;
};
[[nodiscard]] Result<EditedWorld> editWorld(const scene::World& source,
                                            const scene::CodecRegistry& codecs,
                                            const scene::Json& command,
                                            scene::AssetContext* assets = nullptr);
} // namespace somnium::runtime
