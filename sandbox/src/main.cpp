// Assembles the concrete game and engine without exposing SDL to the launcher.
// Bounded runs exercise the ordinary event-driven shutdown path for CI.
#include <somnium/core/log.hpp>
#include <somnium/game/codecs.hpp>
#include <somnium/game/sandbox_game.hpp>
#include <somnium/runtime/engine.hpp>

#include <charconv>
#include <cstdio>
#include <string_view>

int main(int argc, char** argv) {
    somnium::runtime::RunOptions options;
    somnium::runtime::EngineConfig config;
    config.scenePath = "scenes/courtyard.json";
    config.inputPath = "input/courtyard.json";
    for (int index = 1; index < argc; index += 2) {
        if (index + 1 >= argc) {
            std::fputs("Usage: sandbox [--frames N] [--backend vulkan|direct3d12|metal] [--replay "
                       "path] [--capture-step N]\n",
                       stderr);
            return 2;
        }
        const std::string_view key(argv[index]), value(argv[index + 1]);
        if (key == "--backend")
            config.backend = value;
        else if (key == "--replay")
            config.replayPath = value;
        else if (key == "--frames" || key == "--capture-step") {
            auto& count = key == "--frames" ? options.frameLimit : options.captureSteps;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || count == 0)
                return 2;
        } else
            return 2;
    }
    somnium::game::SandboxGame game;
    auto engine = somnium::runtime::Engine::create(config);
    if (!engine) {
        somnium::Log::write("sandbox", somnium::Log::Level::Error, engine.error().message);
        return 1;
    }
    somnium::scene::CodecRegistry codecs;
    auto registered = somnium::scene::registerBuiltinCodecs(codecs);
    if (!registered) {
        somnium::Log::write("sandbox", somnium::Log::Level::Error, registered.error().message);
        return 1;
    }
    registered = somnium::game::registerCodecs(codecs);
    if (!registered) {
        somnium::Log::write("sandbox", somnium::Log::Level::Error, registered.error().message);
        return 1;
    }
    auto result = (*engine)->run(game, codecs, options);
    if (!result) {
        somnium::Log::write("sandbox", somnium::Log::Level::Error, result.error().message);
        return 1;
    }
    std::printf("frames=%llu fixed_steps=%llu alpha=%.6f dropped_seconds=%.6f\n",
                static_cast<unsigned long long>(result->frames),
                static_cast<unsigned long long>(result->fixedSteps), result->interpolationAlpha,
                result->droppedSeconds);
    somnium::Log::flush();
    return 0;
}
