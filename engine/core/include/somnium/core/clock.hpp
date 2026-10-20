// Measures elapsed time with a monotonic standard-library clock.
// Instances own their sampling baseline; civil-time changes cannot affect simulation.
#pragma once

#include <chrono>

namespace somnium::core {
class Clock final {
  public:
    using Source = std::chrono::steady_clock;
    static_assert(Source::is_steady);

    void reset() {
        m_previous = Source::now();
    }
    [[nodiscard]] double sampleSeconds() {
        const auto now = Source::now();
        const auto elapsed = std::chrono::duration<double>(now - m_previous).count();
        m_previous = now;
        return elapsed;
    }

  private:
    Source::time_point m_previous = Source::now();
};
} // namespace somnium::core
