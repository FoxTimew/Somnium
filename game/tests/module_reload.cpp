// Exercises actual game generations, assets and neutral replay without a window or GPU.
// The host imports only runtime; layout and behavior assertions use codecs and descriptors.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <somnium/core/profile.hpp>
#include <somnium/core/sha256.hpp>
#include <somnium/platform/file_reader.hpp>
#include <somnium/platform/input_replay.hpp>
#include <somnium/runtime/reloadable_game.hpp>
#include <thread>

namespace {
using namespace somnium;
void check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "Module proof failed: %s\n", message);
        std::exit(1);
    }
}
template <typename Type> Type must(Result<Type> value) {
    if (!value) {
        std::fprintf(stderr, "Module proof failed: %s\n", value.error().message.c_str());
        std::exit(1);
    }
    return std::move(*value);
}
void must(Result<void> value) {
    if (!value) {
        std::fprintf(stderr, "Module proof failed: %s\n", value.error().message.c_str());
        std::exit(1);
    }
}
struct CountingLoader final : assets::Loader<assets::Model> {
    mutable std::size_t loads = 0;
    assets::ModelLoader loader;
    Result<assets::Model> load(const core::ByteReader& reader,
                               std::string_view path) const override {
        ++loads;
        return loader.load(reader, path);
    }
};
runtime::ModuleStorage character(const runtime::ReloadableGame& game) {
    for (const auto& storage : game.storages())
        if (storage.codec == "game.Character")
            return storage;
    check(false, "Character storage was not declared");
    std::exit(1);
}
void equal(const scene::WorldSnapshot& before, const scene::World& world,
           const runtime::ReloadableGame& game) {
    auto after = must(scene::snapshot(world, game.codecs()));
    check(before.canonical == after.canonical, "canonical state changed across exchange");
    check(before.temporal == after.temporal, "interpolation history changed across exchange");
}
glm::vec3 position(const scene::World& world) {
    return world.registry().get<Transform>(must(world.find("character"))).current.position;
}
float doorProgress(const scene::World& world, const runtime::ReloadableGame& game) {
    const auto* codec = game.codecs().find("game.Door");
    check(codec != nullptr, "Door codec disappeared");
    float progress = 0;
    for (auto entity : world.traverse())
        if (codec->contains(world, entity)) {
            auto data = must(codec->serialize(world, entity));
            progress = std::max(progress, must(scene::readNumber(data, "progress")));
        }
    return progress;
}
} // namespace
int main(int argc, char** argv) {
    using namespace somnium;
    if (argc < 4 || argc > 5) {
        std::fputs("Usage: module-proof <module-root> <asset-root> <report.json> [--tracy]\n",
                   stderr);
        return 2;
    }
    bool tracy = false;
    if (argc == 5) {
        check(std::string_view(argv[4]) == "--tracy", "unknown argument");
#if defined(SOMNIUM_ENABLE_PROFILING)
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (!tracy::GetProfiler().IsConnected() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        check(tracy::GetProfiler().IsConnected(), "Tracy collector did not connect");
        tracy = true;
#else
        check(false, "Tracy proof requires iteration-profiling");
#endif
    }
    auto game = must(runtime::ReloadableGame::create({argv[1]}));
    auto reader = must(platform::FileReader::create(argv[2]));
    CountingLoader loader;
    assets::AssetManager<assets::Model> models(reader, loader);
    scene::AssetContext assets{models};
    auto world = must(scene::load(reader, "scenes/courtyard.json", game->codecs(), &assets));
    input::Input inputs(must(input::Configuration::load(reader, "input/courtyard.json")));
    auto replay = must(platform::InputReplay::load(reader, "replays/courtyard.json"));
    must(game->start(*world, {inputs.configuration(), 0}));
    const auto initial = must(scene::snapshot(*world, game->codecs()));
    std::vector<assets::AssetHandle<assets::Model>> handles;
    for (auto entity : world->traverse())
        if (const auto* mesh = world->registry().try_get<MeshRenderer>(entity))
            handles.push_back(mesh->model);
    const auto originalLoads = loader.loads;
    const auto v1 = character(*game);
    std::uint64_t tick = 0;
    auto step = [&] {
        must(inputs.enqueue(must(replay.next(tick))));
        auto state = must(inputs.advance(tick));
        world->beginStep();
        must(game->fixedUpdate(*world, {1. / 60., tick, state}));
        must(world->propagate());
        ++tick;
        SOMNIUM_PROFILE_FRAME();
    };
    while (tick < 300)
        step();
    const auto before = must(scene::snapshot(*world, game->codecs()));
    const auto progress = doorProgress(*world, *game);
    check(progress > 0 && progress < 1, "replay must exchange a partially open door");
    const scene::WorldSnapshot* preserved[] = {&initial, &before};
    must(game->replace(world, {inputs.configuration(), tick}, true, &assets, preserved, "v2.json"));
    equal(before, *world, *game);
    const auto v2 = character(*game);
    std::printf("GAME_TYPES v1=%u/%zu v2=%u/%zu\n", v1.id, v1.size, v2.id, v2.size);
    check(v1.id != v2.id && v1.size != v2.size, "v2 must change type identity and layout");
    check(loader.loads == originalLoads, "kept model assets were loaded again");
    for (auto handle : handles)
        check(bool(models.get(handle)), "asset handle changed across exchange");
    while (tick < 340)
        step();
    check(doorProgress(*world, *game) == 1.F, "door progress did not continue to fully open");
    const auto start = position(*world);
    step();
    const float distance = glm::length(position(*world) - start);
    check(std::abs(distance - .1F) < .00002F, "v2 movement constant was not applied");
    scene::Json failures = scene::Json::array();
    auto reject = [&](std::string_view manifest, ErrorCode code) {
        const auto state = must(scene::snapshot(*world, game->codecs()));
        const auto inputBefore = inputs.state();
        const auto axes = std::vector<float>(inputBefore.axes.begin(), inputBefore.axes.end());
        auto result = game->replace(world, {inputs.configuration(), tick}, true, &assets, preserved,
                                    manifest);
        check(!result && result.error().code == code, "candidate failure category was unexpected");
        check(game->usable(), "old gameplay was not resumed");
        equal(state, *world, *game);
        check(inputs.state().step == inputBefore.step &&
                  std::equal(axes.begin(), axes.end(), inputs.state().axes.begin()),
              "input state changed during rejection");
        failures.push_back({{"publication", manifest}, {"diagnostic", result.error().message}});
    };
    reject("bad_abi.json", ErrorCode::RestartRequired);
    reject("missing_symbol.json", ErrorCode::Io);
    reject("bad_start.json", ErrorCode::InvalidState);
    reject("bad_codec.json", ErrorCode::InvalidArgument);
    // Missing source assets are reported by the platform loader as Io.
    const auto badAssetState = must(scene::snapshot(*world, game->codecs()));
    auto badAsset = game->replace(world, {inputs.configuration(), tick}, true, &assets, preserved,
                                  "bad_asset.json");
    check(!badAsset && game->usable(), "missing candidate asset did not roll back");
    equal(badAssetState, *world, *game);
    failures.push_back(
        {{"publication", "bad_asset.json"}, {"diagnostic", badAsset.error().message}});
    // A malformed binary has a valid digest, so rejection exercises the native loader.
    const std::string invalidBytes = "This is not a shared library.\n";
    const std::filesystem::path moduleRoot(argv[1]);
    {
        std::ofstream file(moduleRoot / "invalid.bin", std::ios::binary);
        file << invalidBytes;
    }
    const auto digest = core::sha256(std::as_bytes(std::span(invalidBytes)));
    {
        std::ofstream file(moduleRoot / "invalid.json");
        file << scene::Json{{"version", 1},
                            {"binary", {{"path", "invalid.bin"}, {"sha256", digest}}}}
                    .dump();
    }
    reject("invalid.json", ErrorCode::Io);
    const auto loadsAfterFailure = loader.loads;
    for (unsigned cycle = 0; cycle < 8; ++cycle) {
        const auto state = must(scene::snapshot(*world, game->codecs()));
        must(game->replace(world, {inputs.configuration(), tick}, true, &assets, preserved,
                           "v2.json"));
        equal(state, *world, *game);
        step();
    }
    check(loader.loads == loadsAfterFailure, "reload cycles reloaded live models");
    while (tick < 401)
        step();
    check(inputs.state().axis(must(inputs.configuration().axis("MoveZ"))) == 0,
          "the replay release was lost or replayed");
    game->stop(*world);
    auto editing = must(scene::snapshot(*world, game->codecs()));
    must(
        game->replace(world, {inputs.configuration(), tick}, false, &assets, preserved, "v2.json"));
    equal(editing, *world, *game);
    must(game->start(*world, {inputs.configuration(), tick}));
    while (game->imageCount() < 64)
        reject("missing_symbol.json", ErrorCode::Io);
    reject("v2.json", ErrorCode::RestartRequired);
    step();
    game->stop(*world);
    const auto final = must(scene::snapshot(*world, game->codecs()));
    const auto imageCount = game->imageCount(), bytes = game->retainedBytes();
    world.reset();
    for (auto handle : handles)
        check(!models.state(handle), "scene asset request leaked after unload");
    scene::Json report{{"version", 1},
                       {"status", "validated"},
                       {"ticks", tick},
                       {"v1_character_size", v1.size},
                       {"v2_character_size", v2.size},
                       {"v1_type", v1.id},
                       {"v2_type", v2.id},
                       {"partial_door", progress},
                       {"v2_step_distance", distance},
                       {"cycles", 8},
                       {"retained_images", imageCount},
                       {"retained_file_bytes", bytes},
                       {"asset_requests_balanced", true},
                       {"groups_contexts_signals", "validated"},
                       {"tracy_connected", tracy},
                       {"rejections", failures},
                       {"final_canonical", final.canonical}};
    std::ofstream output(argv[3], std::ios::binary);
    output << report.dump(2) << '\n';
    output.close();
    check(bool(output), "could not write module proof report");
    std::printf("MODULE_RELOAD_VALIDATED ticks=%llu images=%zu v1=%zu v2=%zu tracy=%d\n",
                static_cast<unsigned long long>(tick), imageCount, v1.size, v2.size, tracy);
    if (tracy)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
