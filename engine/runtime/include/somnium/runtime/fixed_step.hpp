// Converts measured frame time into bounded fixed simulation work.
// Discarded time is explicit so overload cannot silently cause unbounded catch-up.
#pragma once

#include <somnium/core/result.hpp>

#include <cstdint>

namespace somnium::runtime {
struct StepBatch {
    std::uint32_t steps = 0;
    double alpha = 0.0;
    double droppedSeconds = 0.0;
};

class FixedStep final {
  public:
    [[nodiscard]] Result<StepBatch> advance(double elapsedSeconds);
    void reset();

  private:
    double m_remainder = 0.0;
};
} // namespace somnium::runtime
