// Holds one real device while the external test publishes complete shader generations.
// Every response follows a submitted/read-back frame; stdin never triggers a compiler.
#include "shader_reload.hpp"
#include <SDL3/SDL_log.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <somnium/platform/file_reader.hpp>
#include <somnium/platform/platform.hpp>
#include <somnium/renderer/renderer.hpp>

namespace {
using namespace somnium;
using Json = nlohmann::json;
struct Logs {
    std::atomic<unsigned> errors{0};
    SDL_LogOutputFunction previous = nullptr;
    void* data = nullptr;
    Logs() {
        SDL_GetLogOutputFunction(&previous, &data);
        SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
        SDL_SetLogOutputFunction(callback, this);
    }
    ~Logs() {
        SDL_SetLogOutputFunction(previous, data);
    }
    static void SDLCALL callback(void* context, int category, SDL_LogPriority priority,
                                 const char* message) {
        auto& self = *static_cast<Logs*>(context);
        if (priority >= SDL_LOG_PRIORITY_ERROR)
            ++self.errors;
        if (self.previous)
            self.previous(self.data, category, priority, message);
    }
};
// NOLINTBEGIN(bugprone-macro-parentheses)
#define SOMNIUM_TEST_TRY(name, expression)                                                         \
    auto name = (expression);                                                                      \
    if (!name)                                                                                     \
    return tl::unexpected(name.error())
// NOLINTEND(bugprone-macro-parentheses)
Result<void> run(gpu::Device& device, const char* root, const std::filesystem::path& output,
                 const char* fixture, bool srgb) {
    SOMNIUM_TEST_TRY(reader, platform::FileReader::create(root));
    SOMNIUM_TEST_TRY(rendering, renderer::Renderer::create(device, *reader,
                                                           srgb ? gpu::ColorFormat::Rgba8Srgb
                                                                : gpu::ColorFormat::Rgba8));
    auto& renderer = **rendering;
    runtime::ShaderReload watcher(*reader, renderer);
    double watchClock = 0;
    watcher.poll(watchClock);
    std::ifstream stream(fixture);
    const auto scene = Json::parse(stream, nullptr, false);
    if (!scene.is_object())
        return tl::unexpected(Error{ErrorCode::Io, "Invalid reload fixture"});
    const auto vertices = scene.at("vertices").get<std::vector<std::array<float, 8>>>();
    const auto indices = scene.at("indices").get<std::vector<std::uint16_t>>();
    SOMNIUM_TEST_TRY(
        mesh, renderer.createMesh({std::as_bytes(std::span(vertices)),
                                   std::as_bytes(std::span(indices)), gpu::IndexFormat::Uint16}));
    constexpr std::array white = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
    SOMNIUM_TEST_TRY(texture, renderer.createTexture({1, 1}, white));
    SOMNIUM_TEST_TRY(material, renderer.createMaterial({.texture = *texture}));
    renderer.reserveDraws(1);
    const gpu::FrameSize size{scene.at("width").get<std::uint32_t>(),
                              scene.at("height").get<std::uint32_t>()};
    const auto clearBytes = scene.at("clear_rgba").get<std::array<unsigned, 4>>();
    std::array<float, 4> clear{};
    for (std::size_t i = 0; i < clear.size(); ++i)
        clear[i] = static_cast<float>(clearBytes[i]) / 255.0F;
    SOMNIUM_TEST_TRY(color, device.createTexture({srgb ? gpu::TextureKind::ColorRgba8Srgb
                                                       : gpu::TextureKind::ColorRgba8,
                                                  size.width, size.height, clear}));
    SOMNIUM_TEST_TRY(
        depth, device.createTexture({gpu::TextureKind::Depth32, size.width, size.height, clear}));
    renderer::FrameData frame;
    const auto light = scene.at("light").get<std::array<float, 3>>();
    frame.lightDirection = {light[0], light[1], light[2]};
    const auto ambient = scene.at("ambient").get<std::array<float, 2>>();
    const gpu::RenderPass pass{*color, *depth, clear};
    std::cout << Json({{"event", "ready"},
                       {"device", device.info().name},
                       {"generation", renderer.shaderGeneration()}})
                     .dump()
              << '\n'
              << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto command = Json::parse(line, nullptr, false);
        if (!command.is_object())
            return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid proof command"});
        const auto operation = command.value("op", "");
        if (operation == "quit")
            break;
        if (operation == "watch") {
            watchClock += 0.25;
            watcher.poll(watchClock);
            std::cout << Json({{"event", "watch"},
                               {"diagnostic", watcher.diagnostic()},
                               {"generation", renderer.shaderGeneration()}})
                             .dump()
                      << '\n'
                      << std::flush;
        } else if (operation == "reload") {
            SOMNIUM_TEST_TRY(document, reader->read("shaders/model.program.json"));
            auto result = renderer.reloadShaders(*reader, "shaders/model.program.json", *document);
            std::cout << Json({{"event", "reload"},
                               {"ok", bool(result)},
                               {"changed", result && *result},
                               {"restart_required",
                                !result && result.error().code == ErrorCode::RestartRequired},
                               {"message", result ? "" : result.error().message},
                               {"generation", renderer.shaderGeneration()}})
                             .dump()
                      << '\n'
                      << std::flush;
        } else if (operation == "render") {
            const auto name = command.value("name", "");
            if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") !=
                                    std::string::npos)
                return tl::unexpected(
                    Error{ErrorCode::InvalidArgument, "Unsafe proof output name"});
            for (std::size_t variant = 0; variant < 4; ++variant) {
                renderer::Draw draw{*mesh, *material};
                draw.world[0][0] = variant % 2 == 0 ? 1.0F : -1.0F;
                frame.ambient = ambient[variant / 2];
                SOMNIUM_TEST_TRY(begun, device.beginFrame());
                SOMNIUM_TEST_TRY(recorded, renderer.record(pass, frame, std::span(&draw, 1)));
                SOMNIUM_TEST_TRY(submitted, device.submitFrame());
                SOMNIUM_TEST_TRY(pixels, device.readback(*color));
                std::ofstream image(output / (name + "-" + std::to_string(variant) + ".rgba"),
                                    std::ios::binary);
                image.write(reinterpret_cast<const char*>(pixels->data()),
                            static_cast<std::streamsize>(pixels->size()));
                image.close();
                if (!image)
                    return tl::unexpected(Error{ErrorCode::Io, "Cannot save reload proof image"});
            }
            std::cout << Json({{"event", "render"}, {"name", name}, {"variants", 4}}).dump() << '\n'
                      << std::flush;
        } else
            return tl::unexpected(Error{ErrorCode::InvalidArgument, "Unknown proof operation"});
    }
    SOMNIUM_TEST_TRY(retiredDepth, device.destroy(*depth));
    SOMNIUM_TEST_TRY(retiredColor, device.destroy(*color));
    return {};
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 6 || (std::string_view(argv[5]) != "unorm" && std::string_view(argv[5]) != "srgb"))
        return 2;
    const std::string_view name(argv[1]);
    if (name != "vulkan" && name != "direct3d12" && name != "metal")
        return 2;
    const auto backend = name == "vulkan"  ? gpu::Backend::Vulkan
                         : name == "metal" ? gpu::Backend::Metal
                                           : gpu::Backend::D3D12;
    Logs logs;
    auto platform = platform::Platform::createHeadless();
    if (!platform) {
        std::cerr << platform.error().message << '\n';
        return 77;
    }
    auto device = gpu::Device::create({backend, SOMNIUM_GPU_DEBUG != 0});
    if (!device) {
        std::cerr << device.error().message << '\n';
        return 77;
    }
    const auto result =
        run(**device, argv[2], argv[3], argv[4], std::string_view(argv[5]) == "srgb");
    device->reset();
    platform->reset();
    if (!result) {
        std::cerr << result.error().message << '\n';
        return 1;
    }
    if (logs.errors != 0)
        return 1;
    return 0;
}
