// Defines the lifecycle implemented by the separately compiled game.
// Contexts borrow engine services for a call; game never owns engine state.
#pragma once

#include <somnium/core/byte_reader.hpp>
#include <somnium/core/result.hpp>
#include <somnium/input/input.hpp>
#include <somnium/renderer/camera.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace somnium::runtime {
class GameContext {
  public:
    explicit GameContext(core::ByteReader& reader) : resources(reader) {}
    core::ByteReader& resources;
    glm::vec3 lightDirection{0, 0, 1};
    float ambient = 0;
    std::array<float, 4> clearColor{0, 0, 0, 1};
    [[nodiscard]] Result<void> requestModel(std::string_view logicalPath);
    [[nodiscard]] std::string_view modelPath() const {
        return m_modelPath;
    }

  private:
    std::string m_modelPath;
};
struct FixedUpdate {
    double seconds;
    std::uint64_t tick;
    const input::State& inputs;
};
class Gameplay {
  public:
    virtual ~Gameplay() = default;
    [[nodiscard]] virtual Result<void> start(GameContext& context) = 0;
    virtual void fixedUpdate(const FixedUpdate& update) = 0;
    [[nodiscard]] virtual renderer::Camera camera() const = 0;
    virtual void stop() = 0;
};
} // namespace somnium::runtime
