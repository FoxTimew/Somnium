// Defines the shared numeric and coordinate conventions of the engine.
// SDL viewport/front-face validation belongs to phase 1 and ADR-0002.
#pragma once

#include <cstdint>
#include <limits>

namespace somnium::core {
// Right-handed world, +Y up, meters, camera toward -Z. Column vectors:
// clip = projectionRH_ZO * view * world * position; local = T * R * S.
// Quaternions in memory and scene v2 ([x,y,z,w]); radians at API boundaries.
// Euler degrees only in the editor and v1 migration: extrinsic X,Y,Z (Rz * Ry * Rx).
// Validate squared length in double; never normalize accepted file values or change sign.
inline constexpr double kQuaternionUnitTolerance = 1e-5;
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
