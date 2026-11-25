// Loads the real glTF model and runtime draw list, then captures a windowless renderer frame.
// The independent Python oracle owns acceptance thresholds and reference regeneration.
#include "model_rendering.hpp"
#include <SDL3/SDL_log.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <somnium/platform/file_reader.hpp>
#include <somnium/platform/platform.hpp>
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
    SOMNIUM_TEST_TRY(scene, loadJson(*sceneReader, files.scene.filename().string()));
    SOMNIUM_TEST_TRY(shaderReader,
                     platform::FileReader::create(files.shaders.parent_path().string()));
    SOMNIUM_TEST_TRY(fixtureReader, platform::FileReader::create(files.fixtures.string()));
    assets::ModelLoader loader;
    assets::AssetManager<assets::Model> manager(*fixtureReader, loader);
    SOMNIUM_TEST_TRY(handle, manager.request(scene->at("fixture").get<std::string>()));
    SOMNIUM_TEST_TRY(cpu, manager.get(*handle));
    if (cpu->get().primitives.size() != 7 || !cpu->get().warning.empty())
        return tl::unexpected(Error{ErrorCode::InvalidState, "Cube fixture contract differs"});
    SOMNIUM_TEST_TRY(renderer, renderer::Renderer::create(device, *shaderReader));
    SOMNIUM_TEST_TRY(model, runtime::ModelRendering::create(**renderer, cpu->get()));
    // The last draw is deliberately behind the cube, so painter ordering cannot fake depth.
    if ((*model)->identities().size() != 7 ||
        (*model)->identities().back() != assets::PrimitiveIdentity{1, 0})
        return tl::unexpected(Error{ErrorCode::InvalidState, "Depth witness must be drawn last"});
    for (std::uint32_t i = 0; i < 6; ++i) {
        if ((*model)->identities()[i] != assets::PrimitiveIdentity{0, i})
            return tl::unexpected(Error{ErrorCode::InvalidState, "Primitive identity changed"});
    }
    const gpu::FrameSize size{scene->at("width").get<std::uint32_t>(),
                              scene->at("height").get<std::uint32_t>()};
    const auto vector = [](const Json& value) {
        const auto v = value.get<std::array<float, 3>>();
        return glm::vec3(v[0], v[1], v[2]);
    };
    const auto& camera = scene->at("camera");
    SOMNIUM_TEST_TRY(frame, renderer::cameraFrame(
                                {vector(camera.at("eye")), vector(camera.at("target")),
                                 vector(camera.at("up")), camera.at("vertical_fov").get<float>(),
                                 camera.at("near").get<float>(), camera.at("far").get<float>()},
                                size));
    frame->lightDirection = vector(scene->at("light"));
    const auto clear = scene->at("clear_rgba").get<std::array<float, 4>>();
    const auto clearDepth = scene->at("clear_depth").get<float>();
    SOMNIUM_TEST_TRY(color, device.createTexture({gpu::TextureKind::ColorRgba8, size.width,
                                                  size.height, clear, clearDepth}));
    SOMNIUM_TEST_TRY(depth, device.createTexture({gpu::TextureKind::Depth32, size.width,
                                                  size.height, clear, clearDepth}));
    const gpu::RenderPass pass{*color, *depth, clear, clearDepth};
    // Negative glTF scale reverses winding; the symmetric flat-color fixture keeps one oracle.
    auto mirroredDraws = (*model)->draws();
    const auto reflection = glm::scale(glm::mat4(1), vector(scene->at("mirror_scale")));
    for (auto& draw : mirroredDraws)
        draw.world = reflection * draw.world;
    SOMNIUM_TEST_TRY(mirrorBegin, device.beginFrame());
    SOMNIUM_TEST_TRY(mirrorRecorded, (*renderer)->record(pass, *frame, mirroredDraws));
    SOMNIUM_TEST_TRY(mirrorSubmitted, device.submitFrame());
    SOMNIUM_TEST_TRY(mirrorPixels, device.readback(*color));
    std::ofstream mirrorImage(files.output / "mirrored.rgba", std::ios::binary);
    mirrorImage.write(reinterpret_cast<const char*>(mirrorPixels->data()),
                      static_cast<std::streamsize>(mirrorPixels->size()));
    mirrorImage.close();
    if (!mirrorImage)
        return tl::unexpected(Error{ErrorCode::Io, "Cannot write mirrored cube capture"});
    // A material retains its texture; releasing a model invalidates all draw handles immediately.
    const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                         std::byte{255}};
    SOMNIUM_TEST_TRY(texture, (*renderer)->createTexture({1, 1}, white));
    SOMNIUM_TEST_TRY(material, (*renderer)->createMaterial({.texture = *texture}));
    const auto referenced = (*renderer)->destroy(*texture);
    if (referenced || referenced.error().code != ErrorCode::InvalidState)
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Referenced texture destruction accepted"});
    SOMNIUM_TEST_TRY(retiredMaterial, (*renderer)->destroy(*material));
    SOMNIUM_TEST_TRY(retiredTexture, (*renderer)->destroy(*texture));
    SOMNIUM_TEST_TRY(reused, (*renderer)->createTexture({1, 1}, white));
    if (reused->index != texture->index || reused->generation == texture->generation ||
        (*renderer)->destroy(*texture))
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Renderer generation contract failed"});
    SOMNIUM_TEST_TRY(retiredReused, (*renderer)->destroy(*reused));
    SOMNIUM_TEST_TRY(begin, device.beginFrame());
    auto invalid = (*model)->draws();
    invalid.front().world = glm::mat4(0);
    auto rejected = (*renderer)->record(pass, *frame, invalid);
    if (rejected || rejected.error().code != ErrorCode::InvalidArgument)
        return tl::unexpected(Error{ErrorCode::InvalidState, "Singular world accepted"});
    SOMNIUM_TEST_TRY(recorded, (*renderer)->record(pass, *frame, (*model)->draws()));
    auto staleDraws = (*model)->draws();
    model->reset();
    const auto stale = (*renderer)->record(pass, *frame, staleDraws);
    if (stale || stale.error().code != ErrorCode::InvalidHandle ||
        device.statistics().pendingReleases == 0)
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Renderer retirement contract failed"});
    SOMNIUM_TEST_TRY(submitted, device.submitFrame());
    if (device.statistics().drawCalls != 14 || device.statistics().pendingReleases != 0)
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Expected one draw per primitive and flushed releases"});
    SOMNIUM_TEST_TRY(pixels, device.readback(*color));
    std::ofstream image(files.output / "actual.rgba", std::ios::binary);
    image.write(reinterpret_cast<const char*>(pixels->data()),
                static_cast<std::streamsize>(pixels->size()));
    image.close();
    std::ofstream metadata(files.output / "device.json");
    metadata << Json{{"backend", device.info().driver},
                     {"device", device.info().name},
                     {"driver_version", device.info().version},
                     {"debug", SOMNIUM_GPU_DEBUG != 0},
                     {"resource_checks", "passed"},
                     {"draw_calls", device.statistics().drawCalls},
                     {"output_encoding", "shader sRGB into RGBA8 UNORM"}}
                    .dump(2)
             << '\n';
    metadata.close();
    if (!image || !metadata)
        return tl::unexpected(Error{ErrorCode::Io, "Cannot write cube capture"});
    SOMNIUM_TEST_TRY(retiredDepth, device.destroy(*depth));
    SOMNIUM_TEST_TRY(retiredColor, device.destroy(*color));
    return {};
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "Usage: somnium_renderer_offscreen BACKEND SCENE OUTPUT SHADERS FIXTURES\n";
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
