// Measures the real courtyard snapshot contract on the selected development configuration.
// The report is observational: the accepted undo budget is evaluated at the checkpoint.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <somnium/game/codecs.hpp>
#include <somnium/platform/file_reader.hpp>

namespace {
int fail(const somnium::Error& error) {
    std::fprintf(stderr, "%s\n", error.message.c_str());
    return 1;
}
} // namespace
int main(int argc, char** argv) {
    using namespace somnium;
    if (argc != 3) {
        std::fputs("Usage: somnium_iteration_benchmark <source-root> <report.json>\n", stderr);
        return 2;
    }
    auto reader = platform::FileReader::create(argv[1]);
    if (!reader)
        return fail(reader.error());
    assets::ModelLoader loader;
    assets::AssetManager<assets::Model> models(*reader, loader);
    scene::AssetContext assets{models};
    scene::CodecRegistry codecs;
    auto registered = scene::registerBuiltinCodecs(codecs);
    if (!registered)
        return fail(registered.error());
    registered = game::registerCodecs(codecs);
    if (!registered)
        return fail(registered.error());
    auto world = scene::load(*reader, "scenes/courtyard.json", codecs, &assets);
    if (!world)
        return fail(world.error());
    auto original = scene::serialize(**world, codecs);
    if (!original)
        return fail(original.error());
    std::vector<double> times;
    std::vector<scene::WorldSnapshot> retained;
    times.reserve(200);
    retained.reserve(16);
    for (int index = -20; index < 200; ++index) {
        const auto begin = std::chrono::steady_clock::now();
        auto saved = scene::snapshot(**world, codecs);
        const auto end = std::chrono::steady_clock::now();
        if (!saved)
            return fail(saved.error());
        if (saved->canonical != *original)
            return fail({ErrorCode::InvalidState, "Snapshot changed the canonical scene"});
        if (index >= 0)
            times.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
        if (index >= 184)
            retained.push_back(std::move(*saved));
    }
    std::sort(times.begin(), times.end());
    std::size_t bytes = 0;
    for (const auto& saved : retained)
        bytes += saved.retainedBytes();
    const scene::Json report{
        {"version", 1},
        {"configuration", SOMNIUM_BENCHMARK_BUILD},
        {"scene", "scenes/courtyard.json"},
        {"entities", (*world)->size()},
        {"warmup", 20},
        {"samples", times.size()},
        {"p50_ms", times[99]},
        {"p95_ms", times[189]},
        {"max_ms", times.back()},
        {"retained_16_bytes", bytes},
        {"canonical_bytes", original->size()},
        {"budget_p95_ms", 1},
        {"budget_retained_bytes", std::size_t{16} * 1024 * 1024},
        {"undo_budget_met", times[189] <= 1 && bytes <= std::size_t{16} * 1024 * 1024}};
    const auto text = report.dump(2) + "\n";
    std::ofstream output(argv[2], std::ios::binary);
    output << text;
    output.close();
    if (!output)
        return fail({ErrorCode::Io, "Could not write snapshot measurement"});
    std::puts(text.c_str());
    return 0;
}
