// Defines the lifecycle implemented by the separately compiled game.
// Contexts borrow engine services for a call; game never owns engine state.
#pragma once

#include <somnium/core/byte_reader.hpp>
#include <somnium/core/result.hpp>

#include <cstdint>

namespace somnium::runtime {
struct GameContext {
    core::ByteReader& resources;
};
struct FixedUpdate {
    double seconds;
    std::uint64_t tick;
};
class Gameplay {
  public:
    virtual ~Gameplay() = default;
    [[nodiscard]] virtual Result<void> start(GameContext& context) = 0;
    virtual void fixedUpdate(const FixedUpdate& update) = 0;
    virtual void stop() = 0;
};
} // namespace somnium::runtime
