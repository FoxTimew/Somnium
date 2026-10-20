// Assembles the concrete game and engine without exposing SDL to the launcher.
// Bounded runs exercise the ordinary event-driven shutdown path for CI.
#include <somnium/core/log.hpp>
#include <somnium/game/foundation_game.hpp>
#include <somnium/runtime/engine.hpp>

#include <charconv>
#include <cstdio>
#include <string_view>

int main(int argc, char** argv) {
    somnium::runtime::RunOptions options;
    if (argc != 1) {
        if (argc != 3 || std::string_view(argv[1]) != "--frames") {
            std::fputs("Usage: sandbox [--frames positive-count]\n", stderr);
            return 2;
        }
        const std::string_view count(argv[2]);
        const auto parsed =
            std::from_chars(count.data(), count.data() + count.size(), options.frameLimit);
        if (parsed.ec != std::errc{} || parsed.ptr != count.data() + count.size() ||
            options.frameLimit == 0) {
            std::fputs("Frame count must be a positive integer\n", stderr);
            return 2;
        }
    }
    somnium::game::FoundationGame game;
    auto engine = somnium::runtime::Engine::create();
    if (!engine) {
        somnium::Log::write("sandbox", somnium::Log::Level::Error, engine.error().message);
        return 1;
    }
    auto result = (*engine)->run(game, options);
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
