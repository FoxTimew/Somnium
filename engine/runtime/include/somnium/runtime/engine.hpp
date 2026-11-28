// Exposes engine startup and execution through SDL-free value contracts.
// The implementation owns subsystem lifetimes and calls borrowed gameplay.
#pragma once
#include <somnium/core/export.hpp>

#include <somnium/core/result.hpp>
#include <somnium/runtime/gameplay.hpp>
#include <somnium/scene/serialization.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace somnium::runtime {
#if defined(SOMNIUM_ENABLE_ITERATION)
class ReloadableGame;
#endif
struct EngineConfig {
    std::string_view title = "Somnium";
    int width = 1280;
    int height = 720;
    std::string_view resourceRoot;
    std::string_view scenePath;
    std::string_view inputPath;
    std::string_view replayPath;
    // Empty selects D3D12 on Windows, Metal on macOS and Vulkan on Linux.
    std::string_view backend;
#if defined(SOMNIUM_ENABLE_ITERATION)
    std::string_view sourceRoot;
    std::string_view editorReplayPath;
#endif
};
struct RunOptions {
#if defined(SOMNIUM_ENABLE_ITERATION)
    bool startEditing = false;
    bool iterationReport = false;
    bool watchShaders = false;
    bool watchGameplay = false;
#endif
    // Freeze the replay at this tick. Without captureFrame, present for three seconds.
    std::uint64_t captureSteps = 0;
    bool captureFrame = false;
    std::uint64_t frameLimit =
        0; // Zero means interactive; bounded runs enqueue the normal quit event.
};
struct FrameCapture {
    std::uint32_t width = 0, height = 0;
    bool debugUi = false;
    // Composed frame in display-encoded RGBA8, copied unchanged to the swapchain.
    std::vector<std::byte> rgba;
};
struct RunStats {
    std::uint64_t frames = 0;
    std::uint64_t fixedSteps = 0;
    double droppedSeconds = 0.0;
    double interpolationAlpha = 0.0;
    std::optional<FrameCapture> capture;
#if defined(SOMNIUM_ENABLE_ITERATION)
    std::string editorReport;
#endif
};

class Engine final {
  public:
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<std::unique_ptr<Engine>>
    create(const EngineConfig& config = {});
    SOMNIUM_ENGINE_API ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<RunStats>
    run(Gameplay& game, const scene::CodecRegistry& codecs, RunOptions options = {});
#if defined(SOMNIUM_ENABLE_ITERATION)
    [[nodiscard]] SOMNIUM_ENGINE_API Result<RunStats> run(ReloadableGame& game,
                                                          RunOptions options = {});
#endif

  private:
    struct Impl;
    SOMNIUM_ENGINE_API explicit Engine(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::runtime
