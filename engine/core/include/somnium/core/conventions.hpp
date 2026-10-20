// Defines the shared numeric and coordinate conventions of the engine.
// SDL viewport/front-face validation belongs to phase 1 and ADR-0002.
#pragma once

#include <cstdint>
#include <limits>

namespace somnium::core {
// Right-handed world, +Y up, meters, camera toward -Z. Column vectors:
// clip = projectionRH_ZO * view * world * position; local = T * R * S.
// Normalized quaternions internally; radians at API boundaries.
inline constexpr double kFixedStepSeconds = 1.0 / 60.0;
inline constexpr double kMaxFrameSeconds = 0.250;
inline constexpr std::uint32_t kMaxFixedSteps = 8;
inline constexpr std::uint32_t kInvalidHandleIndex = std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t kInvalidGeneration = 0;
inline constexpr std::uint32_t kFirstGeneration = 1;
inline constexpr std::uint64_t kMaxResourceBytes = 256ULL * 1024ULL * 1024ULL;
inline constexpr float kClipNearDepth = 0.0F;
inline constexpr float kClipFarDepth = 1.0F;
} // namespace somnium::core
