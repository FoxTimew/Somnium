// Drives bounded simulation and VSYNC presentation on the window thread.
// An initialized game's stop hook runs on both successful and failed exits.
#include <somnium/runtime/engine.hpp>

#include <somnium/core/assert.hpp>
#include <somnium/core/clock.hpp>
#include <somnium/core/conventions.hpp>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>
#include <somnium/gpu/device.hpp>
#include <somnium/platform/file_reader.hpp>
#include <somnium/platform/platform.hpp>
#include <somnium/runtime/fixed_step.hpp>

#include <array>
#include <utility>

namespace somnium::runtime {
struct Engine::Impl {
    std::unique_ptr<platform::Platform> platform;
    platform::FileReader resources;
    std::unique_ptr<gpu::Device> gpu;
    gpu::Pipeline pipeline;
    gpu::Buffer vertices;
    gpu::Texture texture;
    gpu::Sampler sampler;
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
    auto name = config.backend;
    if (name.empty()) {
#if defined(_WIN32)
        name = "direct3d12";
#elif defined(__APPLE__)
        name = "metal";
#else
        name = "vulkan";
#endif
    }
    gpu::Backend backend;
    if (name == "vulkan")
        backend = gpu::Backend::Vulkan;
    else if (name == "direct3d12")
        backend = gpu::Backend::D3D12;
    else if (name == "metal")
        backend = gpu::Backend::Metal;
    else
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Unknown graphics backend"});
    auto device = gpu::Device::create({backend, SOMNIUM_GPU_DEBUG != 0});
    if (!device)
        return tl::unexpected(device.error());
    auto format = (*device)->attachWindow(**platform);
    if (!format)
        return tl::unexpected(format.error());
    auto vertex = (*device)->loadShader(*resources, "shaders/passthrough.vsMain.vert.json");
    if (!vertex)
        return tl::unexpected(vertex.error());
    auto fragment = (*device)->loadShader(*resources, "shaders/textured.psMain.frag.json");
    if (!fragment)
        return tl::unexpected(fragment.error());
    gpu::PipelineDescription pipelineDescription{*vertex, *fragment};
    pipelineDescription.color = *format;
    pipelineDescription.depth = gpu::DepthFormat::None;
    pipelineDescription.depthTest = pipelineDescription.depthWrite = false;
    auto pipeline = (*device)->createPipeline(pipelineDescription);
    if (!pipeline)
        return tl::unexpected(pipeline.error());
    auto retiredVertex = (*device)->destroy(*vertex);
    if (!retiredVertex)
        return tl::unexpected(retiredVertex.error());
    auto retiredFragment = (*device)->destroy(*fragment);
    if (!retiredFragment)
        return tl::unexpected(retiredFragment.error());
    const std::array vertices = {gpu::Vertex{{-0.7F, -0.65F, 0.5F}, {1, 0.15F, 0.1F, 1}, {-1, -1}},
                                 gpu::Vertex{{0.7F, -0.65F, 0.5F}, {0.1F, 1, 0.25F, 1}, {-1, -1}},
                                 gpu::Vertex{{0, 0.75F, 0.5F}, {0.15F, 0.4F, 1, 1}, {-1, -1}}};
    const auto bytes = std::as_bytes(std::span(vertices));
    auto buffer = (*device)->createVertexBuffer(static_cast<std::uint32_t>(bytes.size()));
    if (!buffer)
        return tl::unexpected(buffer.error());
    auto uploaded = (*device)->upload(*buffer, bytes);
    if (!uploaded)
        return tl::unexpected(uploaded.error());
    auto texture = (*device)->createTexture({gpu::TextureKind::SampledRgba8, 1, 1});
    if (!texture)
        return tl::unexpected(texture.error());
    const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
    auto texels = (*device)->upload(*texture, std::as_bytes(std::span(white)));
    if (!texels)
        return tl::unexpected(texels.error());
    auto sampler = (*device)->createNearestSampler();
    if (!sampler)
        return tl::unexpected(sampler.error());
    Log::write("gpu", Log::Level::Info, (*device)->info().driver + ": " + (*device)->info().name);
    auto impl =
        std::make_unique<Impl>(Impl{std::move(*platform), std::move(*resources), std::move(*device),
                                    *pipeline, *buffer, *texture, *sampler});
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
    gpu::FrameSize previousSize;
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
        auto begun = m_impl->gpu->beginFrame();
        if (!begun)
            return tl::unexpected(begun.error());
        struct FinishFrame {
            gpu::Device& device;
            bool active = true;
            ~FinishFrame() {
                if (active) {
                    const auto finished = device.finishFrame();
                    SOMNIUM_VERIFY(finished);
                }
            }
        } frame{*m_impl->gpu};
        auto size = m_impl->gpu->acquireSwapchain();
        if (!size)
            return tl::unexpected(size.error());
        // A bounded sandbox run exercises closing after acquisition, before a render pass.
        if (options.frameLimit != 0 && stats.frames >= options.frameLimit) {
            auto quit = m_impl->platform->requestQuit();
            if (!quit)
                return tl::unexpected(quit.error());
        }
        const auto afterAcquire = m_impl->platform->pollEvents();
        if (afterAcquire.quitRequested) {
            frame.active = false;
            auto submitted = m_impl->gpu->submitFrame();
            if (!submitted)
                return tl::unexpected(submitted.error());
            Log::write("runtime", Log::Level::Info,
                       "Close during frame: acquired command buffer submitted");
            break;
        }
        if (afterAcquire.clockResetRequested) {
            clock.reset();
            accumulator.reset();
        }
        if (!size->drawable()) {
            frame.active = false;
            auto submitted = m_impl->gpu->submitFrame();
            if (!submitted)
                return tl::unexpected(submitted.error());
            Log::write("runtime", Log::Level::Info, "Swapchain unavailable: waiting for events");
            // Do not wait indefinitely after consuming a restore event above.
            if (!afterAcquire.clockResetRequested) {
                auto waited = m_impl->platform->waitEvents();
                if (!waited)
                    return tl::unexpected(waited.error());
            }
            clock.reset();
            accumulator.reset();
            continue;
        }
        if (size->width != previousSize.width || size->height != previousSize.height) {
            Log::write("runtime", Log::Level::Info,
                       "Swapchain extent " + std::to_string(size->width) + "x" +
                           std::to_string(size->height));
            previousSize = *size;
        }
        const gpu::Draw draw{
            m_impl->pipeline, m_impl->vertices, m_impl->texture, m_impl->sampler, 0, 3};
        auto rendered =
            m_impl->gpu->recordPass({{}, {}, {0.025F, 0.035F, 0.065F, 1}}, std::span(&draw, 1));
        if (!rendered)
            return tl::unexpected(rendered.error());
        frame.active = false;
        auto submitted = m_impl->gpu->submitFrame();
        if (!submitted)
            return tl::unexpected(submitted.error());
        ++stats.frames;
        stats.interpolationAlpha = batch->alpha;
        stats.droppedSeconds += batch->droppedSeconds;
        SOMNIUM_PROFILE_FRAME();
    }
    return stats;
}
} // namespace somnium::runtime
