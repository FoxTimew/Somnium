// Maintains a bounded timing window without allocating in the frame loop.
// Suspension and invalid samples do not distort the displayed running mean.
#pragma once
#include <array>
#include <cmath>
#include <cstddef>
namespace somnium::debug_ui::detail {
class FrameTimes final {
  public:
    void sample(double seconds) {
        if (!std::isfinite(seconds) || seconds <= 0 || seconds > 0.25)
            return;
        m_sum -= m_values[m_next];
        m_values[m_next] = seconds;
        m_sum += seconds;
        m_next = (m_next + 1) % m_values.size();
        if (m_count < m_values.size())
            ++m_count;
    }
    [[nodiscard]] double milliseconds() const {
        return m_count ? m_sum * 1000.0 / static_cast<double>(m_count) : 0;
    }
    [[nodiscard]] double fps() const {
        return m_sum > 0 ? static_cast<double>(m_count) / m_sum : 0;
    }

  private:
    std::array<double, 120> m_values{};
    std::size_t m_next = 0, m_count = 0;
    double m_sum = 0;
};
} // namespace somnium::debug_ui::detail
