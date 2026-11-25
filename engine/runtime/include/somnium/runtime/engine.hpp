// Exposes engine startup and execution through SDL-free value contracts.
// The implementation owns subsystem lifetimes and calls borrowed gameplay.
#pragma once

#include <somnium/core/result.hpp>
#include <somnium/runtime/gameplay.hpp>
#include <somnium/scene/serialization.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

namespace somnium::runtime {
struct EngineConfig {
    std::string_view title = "Somnium - Phase 3";
    int width = 1280;
    int height = 720;
    std::string_view resourceRoot;
    std::string_view scenePath;
    std::string_view inputPath;
    std::string_view replayPath;
    // Empty selects D3D12 on Windows, Metal on macOS and Vulkan on Linux.
    std::string_view backend;
};
struct RunOptions {
    // Replay capture: freeze after this many ticks, present for three seconds, then quit.
    std::uint64_t captureSteps = 0;
    std::uint64_t frameLimit =
        0; // Zero means interactive; bounded runs enqueue the normal quit event.
};
struct RunStats {
    std::uint64_t frames = 0;
    std::uint64_t fixedSteps = 0;
    double droppedSeconds = 0.0;
    double interpolationAlpha = 0.0;
};

class Engine final {
  public:
    [[nodiscard]] static Result<std::unique_ptr<Engine>> create(const EngineConfig& config = {});
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    [[nodiscard]] Result<RunStats> run(Gameplay& game, const scene::CodecRegistry& codecs,
                                       RunOptions options = {});

  private:
    struct Impl;
    explicit Engine(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::runtime
