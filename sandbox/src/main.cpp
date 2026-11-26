// Assembles the concrete game and engine without exposing SDL to the launcher.
// Bounded runs exercise the ordinary event-driven shutdown path for CI.
#include <somnium/core/log.hpp>
#include <somnium/game/codecs.hpp>
#include <somnium/game/sandbox_game.hpp>
#include <somnium/runtime/engine.hpp>

#include <charconv>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
    somnium::runtime::RunOptions options;
    std::string captureOutput;
    somnium::runtime::EngineConfig config;
    config.scenePath = "scenes/courtyard.json";
    config.inputPath = "input/courtyard.json";
    for (int index = 1; index < argc; index += 2) {
        if (index + 1 >= argc) {
            std::fputs("Usage: sandbox [--frames N] [--backend vulkan|direct3d12|metal] [--replay "
                       "path] [--capture-step N] [--capture-output prefix]\n",
                       stderr);
            return 2;
        }
        const std::string_view key(argv[index]), value(argv[index + 1]);
        if (key == "--backend")
            config.backend = value;
        else if (key == "--replay")
            config.replayPath = value;
        else if (key == "--capture-output") {
            captureOutput = value;
            options.captureFrame = true;
        } else if (key == "--frames" || key == "--capture-step") {
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
    const auto& capturedFrame = result->capture;
    if (capturedFrame) {
        const auto& capture = *capturedFrame;
        std::ofstream pixels(captureOutput + ".rgba", std::ios::binary);
        pixels.write(reinterpret_cast<const char*>(capture.rgba.data()),
                     static_cast<std::streamsize>(capture.rgba.size()));
        pixels.close();
        std::ofstream metadata(captureOutput + ".json");
        const somnium::scene::Json description = {{"version", 1},
                                                  {"width", capture.width},
                                                  {"height", capture.height},
                                                  {"format", "RGBA8"},
                                                  {"encoding", "sRGB"},
                                                  {"steps", result->fixedSteps},
                                                  {"debug_ui", capture.debugUi},
                                                  {"source", "engine-composed-frame"}};
        metadata << description.dump(2) << '\n';
        metadata.close();
        if (!pixels || !metadata) {
            somnium::Log::write("sandbox", somnium::Log::Level::Error,
                                "Could not write frame capture");
            return 1;
        }
    }
    somnium::Log::flush();
    return 0;
}
