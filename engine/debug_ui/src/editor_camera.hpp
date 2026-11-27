// Owns a free camera outside World; its mapping is independent from the game's.
// Runtime only forwards fixed-step input and consumes the resulting pose and lens.
#pragma once
#include <somnium/input/input.hpp>
#include <somnium/scene/world.hpp>
namespace somnium::debug_ui::detail {
class EditorCamera final {
  public:
    [[nodiscard]] static Result<input::Configuration> configuration();
    [[nodiscard]] Result<void> initialize(const scene::World& world,
                                          const input::Configuration& mapping);
    void update(const input::State& state, double seconds);
    [[nodiscard]] const Trs& pose() const {
        return m_pose;
    }
    [[nodiscard]] const Camera& lens() const {
        return m_lens;
    }

  private:
    Trs m_pose;
    Camera m_lens;
    float m_yaw = 0, m_pitch = 0;
    input::AxisId m_x, m_y, m_z, m_lookX, m_lookY, m_dolly;
};
} // namespace somnium::debug_ui::detail
