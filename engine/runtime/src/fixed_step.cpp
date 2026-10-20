// Bounds both incoming elapsed time and work per external frame.
// A relative rounding tolerance keeps exact tick boundaries stable over long runs.
#include <somnium/runtime/fixed_step.hpp>

#include <somnium/core/conventions.hpp>

#include <algorithm>
#include <cmath>

namespace somnium::runtime {
Result<StepBatch> FixedStep::advance(double elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0) {
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Elapsed time must be finite and nonnegative"});
    }
    const auto accepted = std::min(elapsedSeconds, core::kMaxFrameSeconds);
    m_remainder += accepted;
    constexpr double kRoundingTolerance = core::kFixedStepSeconds * 1.0e-9;
    const auto available = static_cast<std::uint32_t>(
        std::floor((m_remainder + kRoundingTolerance) / core::kFixedStepSeconds));
    const auto steps = std::min(available, core::kMaxFixedSteps);
    m_remainder = std::max(0.0, m_remainder - available * core::kFixedStepSeconds);
    return StepBatch{steps, m_remainder / core::kFixedStepSeconds,
                     elapsedSeconds - accepted + (available - steps) * core::kFixedStepSeconds};
}

void FixedStep::reset() {
    m_remainder = 0.0;
}
} // namespace somnium::runtime
