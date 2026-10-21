// Executes the versioned D5 scene without a window, then saves raw RGBA for the Python comparator.
// FileReader injects data access; no shader compiler or reflection library is linked here.
#include <somnium/gpu/device.hpp>
#include <somnium/platform/file_reader.hpp>
#include <somnium/platform/platform.hpp>

#include <SDL3/SDL_log.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
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
#define SOMNIUM_TEST_TRY(name, expression)                                                         \
    auto name = (expression);                                                                      \
    if (!name) {                                                                                   \
        return tl::unexpected(name.error());                                                       \
    }

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

Result<void> execute(gpu::Device& device, const std::filesystem::path& scenePath,
                     const std::filesystem::path& output, const std::filesystem::path& shaderRoot) {
    SOMNIUM_TEST_TRY(sceneReader, platform::FileReader::create(scenePath.parent_path().string()));
    SOMNIUM_TEST_TRY(scene, loadJson(*sceneReader, scenePath.filename().string()));
    SOMNIUM_TEST_TRY(shaderReader, platform::FileReader::create(shaderRoot.string()));
    SOMNIUM_TEST_TRY(
        vertex,
        device.loadShader(*shaderReader, scene->at("shaders").at("vertex").get<std::string>()));
    SOMNIUM_TEST_TRY(
        fragment,
        device.loadShader(*shaderReader, scene->at("shaders").at("fragment").get<std::string>()));
    SOMNIUM_TEST_TRY(pipeline, device.createPipeline(*vertex, *fragment));
    // Pipeline creation consumes shader bytecode synchronously. Releasing these is legal.
    SOMNIUM_TEST_TRY(releasedVertex, device.destroy(*vertex));
    SOMNIUM_TEST_TRY(releasedFragment, device.destroy(*fragment));
    const auto releasedShaders = device.createPipeline(*vertex, *fragment);
    if (releasedShaders || releasedShaders.error().code != ErrorCode::InvalidHandle) {
        return tl::unexpected(Error{ErrorCode::InvalidState, "Released shader handle accepted"});
    }
    const auto width = scene->at("width").get<std::uint32_t>();
    const auto height = scene->at("height").get<std::uint32_t>();
    const auto clearColor = scene->at("clear_rgba").get<std::array<float, 4>>();
    const auto clearDepth = scene->at("clear_depth").get<float>();
    SOMNIUM_TEST_TRY(color, device.createTexture({gpu::TextureKind::ColorRgba8, width, height,
                                                  clearColor, clearDepth}));
    SOMNIUM_TEST_TRY(depth, device.createTexture({gpu::TextureKind::Depth32, width, height,
                                                  clearColor, clearDepth}));
    const auto& textureConfig = scene->at("texture");
    SOMNIUM_TEST_TRY(texture,
                     device.createTexture({gpu::TextureKind::SampledRgba8,
                                           textureConfig.at("width").get<std::uint32_t>(),
                                           textureConfig.at("height").get<std::uint32_t>()}));
    const auto texels = textureConfig.at("rgba").get<std::vector<std::uint8_t>>();
    SOMNIUM_TEST_TRY(uploadedTexture, device.upload(*texture, std::as_bytes(std::span(texels))));
    SOMNIUM_TEST_TRY(sampler, device.createNearestSampler());
    std::vector<gpu::Vertex> vertices;
    for (const auto& item : scene->at("vertices")) {
        vertices.push_back({item.at("position").get<std::array<float, 3>>(),
                            item.at("color").get<std::array<float, 4>>(),
                            item.at("uv").get<std::array<float, 2>>()});
    }
    const auto bytes = std::as_bytes(std::span(vertices));
    SOMNIUM_TEST_TRY(buffer, device.createVertexBuffer(static_cast<std::uint32_t>(bytes.size())));
    SOMNIUM_TEST_TRY(uploadedVertices, device.upload(*buffer, bytes));
    std::vector<gpu::Draw> draws;
    for (const auto& item : scene->at("draws")) {
        draws.push_back({*pipeline, *buffer, *texture, *sampler,
                         item.at("first").get<std::uint32_t>(),
                         item.at("count").get<std::uint32_t>()});
    }
    const gpu::RenderPass pass{*color, *depth, scene->at("clear_rgba").get<std::array<float, 4>>(),
                               scene->at("clear_depth").get<float>()};
    auto invalidDraw = draws.front();
    invalidDraw.vertexCount = static_cast<std::uint32_t>(vertices.size() + 3);
    const auto invalidRange = device.render(pass, std::span(&invalidDraw, 1));
    if (invalidRange || invalidRange.error().code != ErrorCode::InvalidArgument) {
        return tl::unexpected(Error{ErrorCode::InvalidState, "Invalid vertex range accepted"});
    }
    SOMNIUM_TEST_TRY(rendered, device.render(pass, draws));
    // A second upload while the first submission may be in flight exercises cycling.
    SOMNIUM_TEST_TRY(reuploaded, device.upload(*buffer, bytes));
    SOMNIUM_TEST_TRY(renderedAgain, device.render(pass, draws));
    // Immediately retire resources already submitted. SDL defers their native destruction.
    SOMNIUM_TEST_TRY(retiredBuffer, device.destroy(*buffer));
    SOMNIUM_TEST_TRY(retiredTexture, device.destroy(*texture));
    SOMNIUM_TEST_TRY(retiredSampler, device.destroy(*sampler));
    SOMNIUM_TEST_TRY(retiredPipeline, device.destroy(*pipeline));
    SOMNIUM_TEST_TRY(retiredDepth, device.destroy(*depth));
    const auto stale = device.upload(*buffer, bytes);
    const auto doubled = device.destroy(*buffer);
    if (stale || doubled || stale.error().code != ErrorCode::InvalidHandle ||
        doubled.error().code != ErrorCode::InvalidHandle) {
        return tl::unexpected(Error{ErrorCode::InvalidState, "Stale/double release was accepted"});
    }
    SOMNIUM_TEST_TRY(reused, device.createVertexBuffer(static_cast<std::uint32_t>(bytes.size())));
    if (reused->index != buffer->index || reused->generation == buffer->generation) {
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Generation not advanced on slot reuse"});
    }
    SOMNIUM_TEST_TRY(pixels, device.readback(*color));
    std::ofstream image(output / "actual.rgba", std::ios::binary);
    image.write(reinterpret_cast<const char*>(pixels->data()),
                static_cast<std::streamsize>(pixels->size()));
    image.close();
    std::ofstream metadata(output / "device.json");
    metadata << Json{{"backend", device.info().driver},
                     {"device", device.info().name},
                     {"driver_version", device.info().version},
                     {"debug", SOMNIUM_GPU_DEBUG != 0},
                     {"resource_checks", "passed"}}
                    .dump(2)
             << '\n';
    metadata.close();
    if (!image || !metadata) {
        return tl::unexpected(Error{ErrorCode::Io, "Cannot write capture"});
    }
    return {};
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "Usage: somnium_gpu_offscreen BACKEND SCENE OUTPUT SHADERS\n";
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
    const auto result = execute(**device, argv[2], argv[3], argv[4]);
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
