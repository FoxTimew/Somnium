// Drives bounded simulation and VSYNC presentation on the window thread.
// An initialized game's stop hook runs on both successful and failed exits.
#include "scene_rendering.hpp"
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
    scene::AssetContext assets{models};
    std::unique_ptr<scene::World> world;
    std::unique_ptr<SceneRendering> scene;
    std::string scenePath, inputPath;
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
    if (config.scenePath.empty())
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Scene path is required"});
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
    impl->scenePath = config.scenePath;
    impl->inputPath = config.inputPath;
    if (!config.replayPath.empty()) {
        auto replay = platform::InputReplay::load(impl->resources, config.replayPath);
        if (!replay)
            return tl::unexpected(replay.error());
        impl->platform->setReplay(std::move(*replay));
    }
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
    auto overlay = debug_ui::Overlay::create(*impl->platform, *impl->gpu);
    if (!overlay)
        return tl::unexpected(overlay.error());
    impl->overlay = std::move(*overlay);
#endif
    return std::unique_ptr<Engine>(new Engine(std::move(impl)));
}

Result<RunStats> Engine::run(Gameplay& game, const scene::CodecRegistry& codecs,
                             RunOptions options) {
    if (m_impl->hasRun) {
        return tl::unexpected(Error{ErrorCode::InvalidState, "An engine session runs only once"});
    }
    m_impl->hasRun = true;
    auto loaded = scene::load(m_impl->resources, m_impl->scenePath, codecs, &m_impl->assets);
    if (!loaded)
        return tl::unexpected(loaded.error());
    m_impl->world = std::move(*loaded);
    auto& world = *m_impl->world;
    auto mapping = input::Configuration::load(m_impl->resources, m_impl->inputPath);
    if (!mapping)
        return tl::unexpected(mapping.error());
    input::Input input(std::move(*mapping));
    if (options.captureSteps != 0 && !m_impl->platform->replaying())
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Capture requires an input replay"});
    auto started = game.start(world, input.configuration());
    if (!started)
        return tl::unexpected(started.error());
    struct StopGame {
        Gameplay& game;
        scene::World& world;
        ~StopGame() {
            game.stop(world);
        }
    } stopGame{game, world};
    auto propagated = world.propagate();
    if (!propagated)
        return tl::unexpected(propagated.error());
    m_impl->scene = std::make_unique<SceneRendering>(*m_impl->renderer, m_impl->models);
    auto resources = m_impl->scene->prepare(world);
    if (!resources)
        return tl::unexpected(resources.error());
    core::Clock clock;
    FixedStep accumulator;
    RunStats stats;
    double captureSeconds = 0;
    bool captureReady = false;
    gpu::FrameSize previousSize;
    bool firstModelFrame = true;
    // Events collected after acquisition wait for the next UI capture decision.
    std::pmr::vector<core::InputEvent> deferredInput;
    deferredInput.reserve(128);
    for (;;) {
        SOMNIUM_PROFILE_SCOPE("runtime.frame");
        const auto events = m_impl->platform->pollEvents(stats.fixedSteps);
        if (events.quitRequested) {
            break;
        }
        if (events.clockResetRequested) {
            clock.reset();
            accumulator.reset();
        }
        const auto frameSeconds = clock.sampleSeconds();
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
        input.setCapture(m_impl->overlay->update(frameSeconds, world, input.configuration(),
                                                 input.state(), m_impl->platform->replaying()));
#endif
        auto queued = input.enqueue(deferredInput);
        if (!queued)
            return tl::unexpected(queued.error());
        deferredInput.clear();
        queued = input.enqueue(events.input);
        if (!queued)
            return tl::unexpected(queued.error());
        auto batch = accumulator.advance(frameSeconds);
        if (!batch) {
            return tl::unexpected(batch.error());
        }
        for (std::uint32_t step = 0; step < batch->steps; ++step) {
            if (options.captureSteps != 0 && stats.fixedSteps >= options.captureSteps)
                break;
            SOMNIUM_PROFILE_SCOPE("runtime.fixed_update");
            auto replay = m_impl->platform->replayEvents(stats.fixedSteps);
            if (!replay)
                return tl::unexpected(replay.error());
            queued = input.enqueue(*replay);
            if (!queued)
                return tl::unexpected(queued.error());
            auto state = input.advance(stats.fixedSteps);
            if (!state)
                return tl::unexpected(state.error());
            world.beginStep();
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
            m_impl->overlay->applyEdits(world);
#endif
            auto updated =
                game.fixedUpdate(world, {core::kFixedStepSeconds, stats.fixedSteps, *state});
            if (!updated)
                return tl::unexpected(updated.error());
            propagated = world.propagate();
            if (!propagated)
                return tl::unexpected(propagated.error());
            ++stats.fixedSteps;
        }
        const bool frozen = options.captureSteps != 0 && stats.fixedSteps >= options.captureSteps;
        if (captureReady)
            captureSeconds += frameSeconds;
        resources = m_impl->scene->prepare(world);
        if (!resources)
            return tl::unexpected(resources.error());
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
        if ((options.frameLimit != 0 && stats.frames >= options.frameLimit) ||
            captureSeconds >= 3.0) {
            auto quit = m_impl->platform->requestQuit();
            if (!quit)
                return tl::unexpected(quit.error());
        }
        const auto afterAcquire = m_impl->platform->pollEvents(stats.fixedSteps);
        if (afterAcquire.input.size() > deferredInput.capacity())
            Log::write("runtime", Log::Level::Warning,
                       "Input burst grows deferred frame storage: {} events",
                       afterAcquire.input.size());
        deferredInput.assign(afterAcquire.input.begin(), afterAcquire.input.end());
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
        auto prepared = m_impl->overlay->prepare();
        if (!prepared)
            return tl::unexpected(prepared.error());
#endif
        auto extracted =
            m_impl->scene->extract(world, frozen ? 1.F : static_cast<float>(batch->alpha), *size);
        if (!extracted)
            return tl::unexpected(extracted.error());
        const auto& sceneFrame = m_impl->scene->frame();
        auto rendered = m_impl->renderer->record({{}, m_impl->depth, sceneFrame.clearColor},
                                                 sceneFrame.uniforms, sceneFrame.draws);
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
        if (frozen && !captureReady) {
            captureReady = true;
            Log::write("runtime", Log::Level::Info, "CAPTURE_READY steps={}", stats.fixedSteps);
            Log::flush();
        }
        if (firstModelFrame && !sceneFrame.draws.empty()) {
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
