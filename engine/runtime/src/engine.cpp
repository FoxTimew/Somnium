// Drives bounded simulation and VSYNC presentation on the window thread.
// An initialized game's stop hook runs on both successful and failed exits.
#include "model_rendering.hpp"
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
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
#include <somnium/debug_ui/overlay.hpp>
#endif

#include <array>
#include <utility>

namespace somnium::runtime {
struct Engine::Impl {
    std::unique_ptr<platform::Platform> platform;
    platform::FileReader resources;
    std::unique_ptr<gpu::Device> gpu;
    std::unique_ptr<renderer::Renderer> renderer;
    assets::ModelLoader loader;
    assets::AssetManager<assets::Model> models;
    std::unique_ptr<ModelRendering> model;
    gpu::Texture depth{};
    Impl(std::unique_ptr<platform::Platform> window, platform::FileReader reader,
         std::unique_ptr<gpu::Device> device, std::unique_ptr<renderer::Renderer> rendering)
        : platform(std::move(window)), resources(std::move(reader)), gpu(std::move(device)),
          renderer(std::move(rendering)), models(resources, loader) {}
    ~Impl() {
        if (depth)
            (void)gpu->destroy(depth);
    }
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
    std::unique_ptr<debug_ui::Overlay> overlay = nullptr;
#endif
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
    auto format = (*device)->attachWindow(**platform, gpu::Composition::SdrLinear);
    if (!format)
        return tl::unexpected(format.error());
    auto renderer = renderer::Renderer::create(**device, *resources, *format);
    if (!renderer)
        return tl::unexpected(renderer.error());
    Log::write("gpu", Log::Level::Info, (*device)->info().driver + ": " + (*device)->info().name);
    auto impl = std::make_unique<Impl>(std::move(*platform), std::move(*resources),
                                       std::move(*device), std::move(*renderer));
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
    auto overlay = debug_ui::Overlay::create(*impl->platform, *impl->gpu);
    if (!overlay)
        return tl::unexpected(overlay.error());
    impl->overlay = std::move(*overlay);
#endif
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
    if (context.modelPath().empty())
        return tl::unexpected(Error{ErrorCode::InvalidState, "Gameplay did not request a model"});
    const auto requested = m_impl->models.request(context.modelPath());
    if (!requested)
        return tl::unexpected(requested.error());
    const auto prepareModel = [&]() -> Result<bool> {
        if (m_impl->model)
            return false;
        const auto state = m_impl->models.state(*requested);
        if (!state)
            return tl::unexpected(state.error());
        if (*state == assets::State::Failed) {
            const auto error = m_impl->models.error(*requested);
            if (!error)
                return tl::unexpected(error.error());
            return tl::unexpected(
                Error{error->code, "Model load failed: " + std::string(context.modelPath()) + ": " +
                                       error->message});
        }
        if (*state != assets::State::Ready)
            return false;
        const auto asset = m_impl->models.get(*requested);
        if (!asset)
            return tl::unexpected(asset.error());
        auto prepared = ModelRendering::create(*m_impl->renderer, asset->get());
        if (!prepared)
            return tl::unexpected(prepared.error());
        m_impl->model = std::move(*prepared);
        Log::write("runtime", Log::Level::Info, "Model ready: {} ({} draws)", context.modelPath(),
                   m_impl->model->draws().size());
        return true;
    };
    core::Clock clock;
    FixedStep accumulator;
    RunStats stats;
    input::Input input;
    gpu::FrameSize previousSize;
    bool firstModelFrame = true;
    for (;;) {
        SOMNIUM_PROFILE_SCOPE("runtime.frame");
        const auto events = m_impl->platform->pollEvents(stats.fixedSteps);
        auto queued = input.enqueue(events.input);
        if (!queued)
            return tl::unexpected(queued.error());
        if (events.quitRequested) {
            break;
        }
        if (events.clockResetRequested) {
            clock.reset();
            accumulator.reset();
        }
        auto preparedModel = prepareModel();
        if (!preparedModel)
            return tl::unexpected(preparedModel.error());
        if (*preparedModel) {
            clock.reset();
            accumulator.reset();
        }
        const auto frameSeconds = clock.sampleSeconds();
        auto batch = accumulator.advance(frameSeconds);
        if (!batch) {
            return tl::unexpected(batch.error());
        }
        for (std::uint32_t step = 0; step < batch->steps; ++step) {
            SOMNIUM_PROFILE_SCOPE("runtime.fixed_update");
            auto state = input.advance(stats.fixedSteps);
            if (!state)
                return tl::unexpected(state.error());
            game.fixedUpdate({core::kFixedStepSeconds, stats.fixedSteps, *state});
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
        const auto afterAcquire = m_impl->platform->pollEvents(stats.fixedSteps);
        queued = input.enqueue(afterAcquire.input);
        if (!queued)
            return tl::unexpected(queued.error());
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
            auto depth =
                m_impl->gpu->createTexture({gpu::TextureKind::Depth32, size->width, size->height});
            if (!depth)
                return tl::unexpected(depth.error());
            if (m_impl->depth) {
                auto retired = m_impl->gpu->destroy(m_impl->depth);
                if (!retired) {
                    (void)m_impl->gpu->destroy(*depth);
                    return tl::unexpected(retired.error());
                }
            }
            m_impl->depth = *depth;
            previousSize = *size;
        }
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
        auto prepared = m_impl->overlay->prepare(frameSeconds);
        if (!prepared)
            return tl::unexpected(prepared.error());
#endif
        auto uniforms = renderer::cameraFrame(game.camera(), *size);
        if (!uniforms)
            return tl::unexpected(uniforms.error());
        uniforms->lightDirection = context.lightDirection;
        uniforms->ambient = context.ambient;
        const std::span<const renderer::Draw> draws =
            m_impl->model ? std::span<const renderer::Draw>(m_impl->model->draws())
                          : std::span<const renderer::Draw>{};
        auto rendered =
            m_impl->renderer->record({{}, m_impl->depth, context.clearColor}, *uniforms, draws);
        if (!rendered)
            return tl::unexpected(rendered.error());
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
        auto overlayRendered = m_impl->overlay->render();
        if (!overlayRendered)
            return tl::unexpected(overlayRendered.error());
#endif
        frame.active = false;
        auto submitted = m_impl->gpu->submitFrame();
        if (!submitted)
            return tl::unexpected(submitted.error());
        ++stats.frames;
        if (firstModelFrame && m_impl->model) {
            Log::write("runtime", Log::Level::Info, "First model frame presented");
            Log::flush();
            firstModelFrame = false;
        }
        stats.interpolationAlpha = batch->alpha;
        stats.droppedSeconds += batch->droppedSeconds;
        SOMNIUM_PROFILE_FRAME();
    }
    return stats;
}
} // namespace somnium::runtime
