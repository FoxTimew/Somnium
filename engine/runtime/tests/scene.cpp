// Loads a serialized hierarchy and captures previous, interpolated and current scene poses.
// The independent Python oracle owns acceptance thresholds and reference regeneration.
#include "scene_rendering.hpp"
#include <SDL3/SDL_log.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <somnium/platform/file_reader.hpp>
#include <somnium/platform/platform.hpp>
#include <somnium/scene/serialization.hpp>
#include <string_view>
namespace {
using namespace somnium;
using Json = nlohmann::json;
struct Logs {
    std::atomic<unsigned> errors{0};
    SDL_LogOutputFunction previous = nullptr;
    void* previousData = nullptr;
    Logs() {
        SDL_GetLogOutputFunction(&previous, &previousData);
        SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
        SDL_SetLogOutputFunction(callback, this);
    }
    ~Logs() {
        SDL_SetLogOutputFunction(previous, previousData);
    }
    static void SDLCALL callback(void* context, int category, SDL_LogPriority priority,
                                 const char* message) {
        auto& self = *static_cast<Logs*>(context);
        if (priority >= SDL_LOG_PRIORITY_ERROR) {
            ++self.errors;
        }
        if (self.previous) {
            self.previous(self.previousData, category, priority, message);
        }
    }
};
// Each failure unwinds ordinary RAII objects before main reports it.
// 'name' declares an identifier, not an expression that could change precedence.
// NOLINTBEGIN(bugprone-macro-parentheses)
#define SOMNIUM_TEST_TRY(name, expression)                                                         \
    auto name = (expression);                                                                      \
    if (!name) {                                                                                   \
        return tl::unexpected(name.error());                                                       \
    }
// NOLINTEND(bugprone-macro-parentheses)

Result<Json> loadJson(const core::ByteReader& reader, std::string_view name) {
    SOMNIUM_TEST_TRY(bytes, reader.read(name));
    auto parsed =
        Json::parse(reinterpret_cast<const char*>(bytes->data()),
                    reinterpret_cast<const char*>(bytes->data()) + bytes->size(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return tl::unexpected(Error{ErrorCode::Io, "Invalid JSON input"});
    }
    return parsed;
}

struct TestFiles {
    std::filesystem::path scene, output, shaders, fixtures;
};
Result<void> execute(gpu::Device& device, const TestFiles& files) {
    SOMNIUM_TEST_TRY(sceneReader, platform::FileReader::create(files.scene.parent_path().string()));
    SOMNIUM_TEST_TRY(config, loadJson(*sceneReader, files.scene.filename().string()));
    SOMNIUM_TEST_TRY(shaderReader,
                     platform::FileReader::create(files.shaders.parent_path().string()));
    SOMNIUM_TEST_TRY(fixtureReader, platform::FileReader::create(files.fixtures.string()));
    assets::ModelLoader loader;
    assets::AssetManager<assets::Model> manager(*fixtureReader, loader);
    scene::AssetContext context{manager};
    scene::CodecRegistry codecs;
    SOMNIUM_TEST_TRY(registered, scene::registerBuiltinCodecs(codecs));
    SOMNIUM_TEST_TRY(
        world, scene::load(*sceneReader, config->at("scene").get<std::string>(), codecs, &context));
    SOMNIUM_TEST_TRY(renderer, renderer::Renderer::create(device, *shaderReader));
    runtime::SceneRendering rendering(**renderer, manager);
    SOMNIUM_TEST_TRY(prepared, rendering.prepare(**world));
    SOMNIUM_TEST_TRY(pivot, (*world)->find(config->at("pivot").get<std::string>()));
    (*world)->beginStep();
    auto local = (*world)->registry().get<Transform>(*pivot).current;
    const auto angles = config->at("next_rotation_degrees").get<std::array<float, 3>>();
    local.rotation = glm::angleAxis(glm::radians(angles[2]), glm::vec3(0, 0, 1)) *
                     glm::angleAxis(glm::radians(angles[1]), glm::vec3(0, 1, 0)) *
                     glm::angleAxis(glm::radians(angles[0]), glm::vec3(1, 0, 0));
    SOMNIUM_TEST_TRY(moved, (*world)->setLocal(*pivot, local));
    SOMNIUM_TEST_TRY(propagated, (*world)->propagate());
    const gpu::FrameSize size{config->at("width").get<std::uint32_t>(),
                              config->at("height").get<std::uint32_t>()};
    SOMNIUM_TEST_TRY(color,
                     device.createTexture({gpu::TextureKind::ColorRgba8, size.width, size.height}));
    SOMNIUM_TEST_TRY(depth,
                     device.createTexture({gpu::TextureKind::Depth32, size.width, size.height}));
    for (const auto& pose : config->at("poses")) {
        SOMNIUM_TEST_TRY(extracted,
                         rendering.extract(**world, pose.at("alpha").get<float>(), size));
        const auto& frame = rendering.frame();
        if (frame.draws.size() != 7)
            return tl::unexpected(
                Error{ErrorCode::InvalidState, "Scene fixture must contain seven draws"});
        SOMNIUM_TEST_TRY(begin, device.beginFrame());
        SOMNIUM_TEST_TRY(recorded, (*renderer)->record({*color, *depth, frame.clearColor},
                                                       frame.uniforms, frame.draws));
        SOMNIUM_TEST_TRY(submitted, device.submitFrame());
        SOMNIUM_TEST_TRY(pixels, device.readback(*color));
        std::ofstream image(files.output / (pose.at("name").get<std::string>() + ".rgba"),
                            std::ios::binary);
        image.write(reinterpret_cast<const char*>(pixels->data()),
                    static_cast<std::streamsize>(pixels->size()));
        image.close();
        if (!image)
            return tl::unexpected(Error{ErrorCode::Io, "Cannot write scene capture"});
    }
    std::ofstream metadata(files.output / "device.json");
    metadata << Json{{"backend", device.info().driver},
                     {"device", device.info().name},
                     {"driver_version", device.info().version},
                     {"debug", SOMNIUM_GPU_DEBUG != 0},
                     {"draw_calls", device.statistics().drawCalls},
                     {"poses", 3},
                     {"output_encoding", "shader sRGB into RGBA8 UNORM"}}
                    .dump(2)
             << '\n';
    metadata.close();
    if (!metadata)
        return tl::unexpected(Error{ErrorCode::Io, "Cannot write scene metadata"});
    SOMNIUM_TEST_TRY(retiredDepth, device.destroy(*depth));
    SOMNIUM_TEST_TRY(retiredColor, device.destroy(*color));
    return {};
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "Usage: somnium_scene_offscreen BACKEND SCENE OUTPUT SHADERS FIXTURES\n";
        return 2;
    }
    const std::string_view name(argv[1]);
    gpu::Backend backend;
    if (name == "vulkan") {
        backend = gpu::Backend::Vulkan;
    } else if (name == "direct3d12") {
        backend = gpu::Backend::D3D12;
    } else if (name == "metal") {
        backend = gpu::Backend::Metal;
    } else {
        return 2;
    }
    Logs logs;
    auto platform = platform::Platform::createHeadless();
    if (!platform) {
        std::cerr << "DEVICE_UNAVAILABLE: " << platform.error().message << '\n';
        return 77;
    }
    auto device = gpu::Device::create({backend, SOMNIUM_GPU_DEBUG != 0});
    if (!device) {
        std::cerr << "DEVICE_UNAVAILABLE: " << device.error().message << '\n';
        return 77;
    }
    const auto result = execute(
        **device, {.scene = argv[2], .output = argv[3], .shaders = argv[4], .fixtures = argv[5]});
    device->reset();
    platform->reset();
    if (!result) {
        std::cerr << "D5_FAILED: " << result.error().message << '\n';
        return 1;
    }
    if (logs.errors != 0) {
        std::cerr << "VALIDATION_ERRORS: " << logs.errors << '\n';
        return 1;
    }
    std::cout << "D5_CAPTURE_COMPLETE\n";
    return 0;
}
