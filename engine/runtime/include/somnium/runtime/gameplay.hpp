// Borrows the scene and fixed-step input; gameplay has no renderer dependency.
// The caller owns World and all services for the entire gameplay lifecycle.
#pragma once
#include <cstdint>
#include <somnium/input/input.hpp>
#include <somnium/scene/world.hpp>

namespace somnium::runtime {
struct StartContext {
    const input::Configuration& mapping;
    std::uint64_t tick = 0;
};
struct FixedUpdate {
    double seconds;
    std::uint64_t tick;
    const input::State& inputs;
};
class Gameplay {
  public:
    virtual ~Gameplay() = default;
    // A failed start may be followed by stop for partial initialization cleanup.
    [[nodiscard]] virtual Result<void> start(scene::World& world, const StartContext& context) = 0;
    [[nodiscard]] virtual Result<void> fixedUpdate(scene::World& world,
                                                   const FixedUpdate& update) = 0;
    virtual void stop(scene::World& world) = 0;
};
} // namespace somnium::runtime
