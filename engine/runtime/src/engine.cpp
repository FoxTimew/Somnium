// Implements the window-only runtime with bounded simulation and variable frames.
// An initialized game's stop hook runs on both successful and failed exits.
#include <somnium/runtime/engine.hpp>

#include <somnium/core/clock.hpp>
#include <somnium/core/conventions.hpp>
#include <somnium/core/profile.hpp>
#include <somnium/platform/file_reader.hpp>
#include <somnium/platform/platform.hpp>
#include <somnium/runtime/fixed_step.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace somnium::runtime {
struct Engine::Impl {
    std::unique_ptr<platform::Platform> platform;
    platform::FileReader resources;
    bool hasRun = false;
};

Engine::Engine(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {
    SOMNIUM_PROFILE_ALLOC(this, sizeof(Engine));
}
Engine::~Engine() {
    SOMNIUM_PROFILE_FREE(this);
}

Result<std::unique_ptr<Engine>> Engine::create(const EngineConfig& config) {
    auto platform = platform::Platform::create({config.title, config.width, config.height});
    if (!platform) {
        return tl::unexpected(platform.error());
    }
    auto resources = platform::FileReader::create(config.resourceRoot);
    if (!resources) {
        return tl::unexpected(resources.error());
    }
    auto impl = std::make_unique<Impl>(Impl{std::move(*platform), std::move(*resources)});
    return std::unique_ptr<Engine>(new Engine(std::move(impl)));
}

Result<RunStats> Engine::run(Gameplay& game, RunOptions options) {
    if (m_impl->hasRun) {
        return tl::unexpected(Error{ErrorCode::InvalidState, "An engine session runs only once"});
    }
    m_impl->hasRun = true;
    GameContext context{m_impl->resources};
    auto started = game.start(context);
    if (!started) {
        return tl::unexpected(started.error());
    }
    struct StopGame {
        Gameplay& game;
        ~StopGame() {
            game.stop();
        }
    } stopGame{game};
    core::Clock clock;
    FixedStep accumulator;
    RunStats stats;
    for (;;) {
        SOMNIUM_PROFILE_SCOPE("runtime.frame");
        const auto events = m_impl->platform->pollEvents();
        if (events.quitRequested) {
            break;
        }
        if (events.clockResetRequested) {
            clock.reset();
            accumulator.reset();
        }
        auto batch = accumulator.advance(clock.sampleSeconds());
        if (!batch) {
            return tl::unexpected(batch.error());
        }
        for (std::uint32_t step = 0; step < batch->steps; ++step) {
            SOMNIUM_PROFILE_SCOPE("runtime.fixed_update");
            game.fixedUpdate({core::kFixedStepSeconds, stats.fixedSteps});
            ++stats.fixedSteps;
        }
        ++stats.frames;
        stats.interpolationAlpha = batch->alpha;
        stats.droppedSeconds += batch->droppedSeconds;
        SOMNIUM_PROFILE_FRAME();
        if (options.frameLimit != 0 && stats.frames >= options.frameLimit) {
            auto quit = m_impl->platform->requestQuit();
            if (!quit) {
                return tl::unexpected(quit.error());
            }
        } else {
            const auto sleepMs = std::ceil((1.0 - batch->alpha) * core::kFixedStepSeconds * 1000.0);
            m_impl->platform->wait(static_cast<std::uint32_t>(std::clamp(sleepMs, 1.0, 17.0)));
        }
    }
    return stats;
}
} // namespace somnium::runtime
