// Drives bounded simulation and VSYNC presentation on the window thread.
// An initialized game's stop hook runs on both successful and failed exits.
#include "scene_rendering.hpp"
#if defined(SOMNIUM_ENABLE_ITERATION)
#include "edit_session.hpp"
#endif
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
    gpu::Texture captureTarget{};
    gpu::ColorFormat presentationFormat{};
    Impl(std::unique_ptr<platform::Platform> window, platform::FileReader reader,
         std::unique_ptr<gpu::Device> device, std::unique_ptr<renderer::Renderer> rendering)
        : platform(std::move(window)), resources(std::move(reader)), gpu(std::move(device)),
          renderer(std::move(rendering)), models(resources, loader) {}
    ~Impl() {
        if (captureTarget)
            (void)gpu->destroy(captureTarget);
        if (depth)
            (void)gpu->destroy(depth);
    }
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
    std::unique_ptr<debug_ui::Overlay> overlay = nullptr;
#endif
    bool hasRun = false;
#if defined(SOMNIUM_ENABLE_ITERATION)
    std::optional<platform::SourceFiles> sourceFiles;
    std::string editorReplayPath;
#endif
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
#if defined(SOMNIUM_ENABLE_ITERATION)
    if (!config.resourceRoot.empty())
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Iteration uses sourceRoot instead of resourceRoot"});
    auto sourceFiles = platform::SourceFiles::create(config.sourceRoot);
    if (!sourceFiles)
        return tl::unexpected(sourceFiles.error());
    auto resources = sourceFiles->dataReader();
#else
    auto resources = platform::FileReader::create(config.resourceRoot);
#endif
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
    impl->presentationFormat = *format;
    impl->inputPath = config.inputPath;
#if defined(SOMNIUM_ENABLE_ITERATION)
    impl->sourceFiles = std::move(*sourceFiles);
    impl->editorReplayPath = config.editorReplayPath;
#endif
    if (!config.replayPath.empty()) {
        auto replay = platform::InputReplay::load(impl->resources, config.replayPath);
        if (!replay)
            return tl::unexpected(replay.error());
        impl->platform->setReplay(std::move(*replay));
    }
#if defined(SOMNIUM_ENABLE_ITERATION)
    else if (!config.editorReplayPath.empty()) {
        auto replay = platform::InputReplay::parse(R"({"version":1,"events":[]})");
        if (!replay)
            return tl::unexpected(replay.error());
        impl->platform->setReplay(std::move(*replay));
    }
#endif
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
    if (options.captureFrame && options.captureSteps == 0)
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Frame capture requires a replay tick"});
    auto loaded = scene::load(m_impl->resources, m_impl->scenePath, codecs, &m_impl->assets);
    if (!loaded)
        return tl::unexpected(loaded.error());
    m_impl->world = std::move(*loaded);
    auto mapping = input::Configuration::load(m_impl->resources, m_impl->inputPath);
    if (!mapping)
        return tl::unexpected(mapping.error());
    input::Input input(std::move(*mapping));
    if (options.captureSteps != 0 && !m_impl->platform->replaying())
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Capture requires an input replay"});
#if defined(SOMNIUM_ENABLE_ITERATION)
    IterationSession session(m_impl->world, game, codecs, input.configuration(), &m_impl->assets);
    EditSession editor(m_impl->world, session, codecs, *m_impl->sourceFiles, m_impl->scenePath,
                       &m_impl->assets);
    auto baseline = editor.initialize();
    if (!baseline)
        return tl::unexpected(baseline.error());
    m_impl->overlay->configureEditor(codecs);
    std::vector<EditorReplayEntry> editorReplay;
    std::size_t editorCursor = 0;
    if (!m_impl->editorReplayPath.empty()) {
        auto bytes = m_impl->resources.read(m_impl->editorReplayPath);
        if (!bytes)
            return tl::unexpected(bytes.error());
        auto replay = parseEditorReplay(
            std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
        if (!replay)
            return tl::unexpected(replay.error());
        editorReplay = std::move(*replay);
    }
    scene::Json report;
    if (options.iterationReport) {
        auto initial = scene::serialize(*m_impl->world, codecs);
        if (!initial)
            return tl::unexpected(initial.error());
        report = {
            {"version", 1}, {"initial", std::move(*initial)}, {"events", scene::Json::array()}};
    }
    if (!options.startEditing) {
        auto started = session.play(0);
        if (!started)
            return tl::unexpected(started.error());
    }
    auto toolMapping = debug_ui::Overlay::iterationMapping();
    if (!toolMapping)
        return tl::unexpected(toolMapping.error());
    std::unique_ptr<input::Input> toolInput;
    if (options.startEditing)
        toolInput = std::make_unique<input::Input>(*toolMapping);
    auto initialized = m_impl->overlay->initializeToolCamera(*m_impl->world, *toolMapping);
    if (!initialized)
        return tl::unexpected(initialized.error());
#else
    auto started = game.start(*m_impl->world, {input.configuration(), 0});
    if (!started)
        return tl::unexpected(started.error());
    struct StopGame {
        Gameplay& game;
        std::unique_ptr<scene::World>& world;
        ~StopGame() {
            game.stop(*world);
        }
    } stopGame{game, m_impl->world};
#endif
    auto propagated = m_impl->world->propagate();
    if (!propagated)
        return tl::unexpected(propagated.error());
    auto enqueue = [&](std::span<const core::InputEvent> events) -> Result<void> {
        auto queued = input.enqueue(events);
        if (!queued)
            return queued;
#if defined(SOMNIUM_ENABLE_ITERATION)
        if (toolInput)
            return toolInput->enqueue(events);
#endif
        return {};
    };
    m_impl->scene = std::make_unique<SceneRendering>(*m_impl->renderer, m_impl->models);
    auto resources = m_impl->scene->prepare(*m_impl->world);
    if (!resources)
        return tl::unexpected(resources.error());
    core::Clock clock;
    FixedStep accumulator;
    RunStats stats;
    double captureSeconds = 0;
    bool captureReady = false;
    std::uint32_t frozenFrames = 0;
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
#if defined(SOMNIUM_ENABLE_ITERATION)
        m_impl->overlay->iterationState(session.playing(), editor.dirty(), editor.revision(),
                                        editor.error());
        m_impl->overlay->historyState(
            {editor.undoCount(), editor.redoCount(), editor.historyBytes()});
#endif
        const auto captureInput =
            m_impl->overlay->update(frameSeconds, *m_impl->world, input.configuration(),
                                    input.state(), m_impl->platform->replaying());
        input.setCapture(captureInput);
#if defined(SOMNIUM_ENABLE_ITERATION)
        if (toolInput)
            toolInput->setCapture(captureInput);
#endif
#endif
        auto queued = enqueue(deferredInput);
        if (!queued)
            return tl::unexpected(queued.error());
        deferredInput.clear();
        queued = enqueue(events.input);
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
            queued = enqueue(*replay);
            if (!queued)
                return tl::unexpected(queued.error());
            auto state = input.advance(stats.fixedSteps);
            if (!state)
                return tl::unexpected(state.error());
#if defined(SOMNIUM_ENABLE_ITERATION)
            const auto command = [&](const scene::Json& value) -> Result<EditEffect> {
                const bool wasPlaying = session.playing();
                auto effect = editor.execute(value, stats.fixedSteps);
                if (!effect)
                    Log::write("iteration", Log::Level::Warning, effect.error().message);
                else {
                    if (effect->selected) {
                        m_impl->overlay->selectEntity(*effect->selected);
                        m_impl->overlay->selectComponent(effect->component);
                    }
                    if (wasPlaying != session.playing()) {
                        if (session.playing())
                            toolInput.reset();
                        else
                            toolInput = std::make_unique<input::Input>(
                                *toolMapping, std::pmr::get_default_resource(), stats.fixedSteps);
                    }
                }
                return effect;
            };
            for (const auto& text : m_impl->overlay->takeCommands())
                (void)command(scene::Json::parse(text, nullptr, false));
            while (editorCursor < editorReplay.size() &&
                   editorReplay[editorCursor].step <= stats.fixedSteps) {
                const auto& entry = editorReplay[editorCursor++];
                if (entry.step != stats.fixedSteps)
                    return tl::unexpected(
                        Error{ErrorCode::InvalidState, "Editor replay step was missed"});
                auto effect = command(entry.command);
                const bool expected =
                    entry.errorContains.empty()
                        ? bool(effect)
                        : !effect &&
                              effect.error().message.find(entry.errorContains) != std::string::npos;
                if (!expected)
                    return tl::unexpected(Error{
                        ErrorCode::InvalidState,
                        "Editor replay expectation failed: " +
                            (effect ? std::string("unexpected success") : effect.error().message)});
                if (options.iterationReport) {
                    auto canonical = scene::serialize(*m_impl->world, codecs);
                    if (!canonical)
                        return tl::unexpected(canonical.error());
                    report["events"].push_back({{"step", entry.step},
                                                {"command", entry.command},
                                                {"accepted", bool(effect)},
                                                {"error", effect ? "" : effect.error().message},
                                                {"playing", session.playing()},
                                                {"dirty", editor.dirty()},
                                                {"canonical", std::move(*canonical)}});
                }
            }
            if (session.playing()) {
#endif
                m_impl->world->beginStep();
                auto updated = game.fixedUpdate(
                    *m_impl->world, {core::kFixedStepSeconds, stats.fixedSteps, *state});
                if (!updated)
                    return tl::unexpected(updated.error());
#if defined(SOMNIUM_ENABLE_ITERATION)
            } else {
                auto toolState = toolInput->advance(stats.fixedSteps);
                if (!toolState)
                    return tl::unexpected(toolState.error());
                m_impl->overlay->updateToolCamera(*toolState, core::kFixedStepSeconds);
            }
#endif
            propagated = m_impl->world->propagate();
            if (!propagated)
                return tl::unexpected(propagated.error());
            ++stats.fixedSteps;
        }
        const bool frozen = options.captureSteps != 0 && stats.fixedSteps >= options.captureSteps;
        if (captureReady)
            captureSeconds += frameSeconds;
        resources = m_impl->scene->prepare(*m_impl->world);
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
            stats.capture.has_value() || captureSeconds >= 3.0) {
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
        const SceneView* view = nullptr;
#if defined(SOMNIUM_ENABLE_ITERATION)
        std::optional<SceneView> toolView;
        if (!session.playing()) {
            toolView.emplace(m_impl->overlay->toolPose(), m_impl->overlay->toolLens());
            view = &*toolView;
        }
#endif
        auto extracted = m_impl->scene->extract(
            *m_impl->world, frozen || view ? 1.F : static_cast<float>(batch->alpha), *size, view);
        if (!extracted)
            return tl::unexpected(extracted.error());
        const auto& sceneFrame = m_impl->scene->frame();
        // Frozen frames prime the UI from the exact requested fixed-step state.
        // Iteration also lets a selection's deferred hierarchy scroll take effect.
        const bool capture = options.captureFrame && captureReady;
        if (capture) {
            gpu::TextureKind kind{};
            switch (m_impl->presentationFormat) {
            case gpu::ColorFormat::Rgba8:
                kind = gpu::TextureKind::ColorRgba8;
                break;
            case gpu::ColorFormat::Bgra8:
                kind = gpu::TextureKind::ColorBgra8;
                break;
            case gpu::ColorFormat::Rgba8Srgb:
                kind = gpu::TextureKind::ColorRgba8Srgb;
                break;
            case gpu::ColorFormat::Bgra8Srgb:
                kind = gpu::TextureKind::ColorBgra8Srgb;
                break;
            }
            auto target = m_impl->gpu->createTexture(
                {kind, size->width, size->height, sceneFrame.clearColor});
            if (!target)
                return tl::unexpected(target.error());
            m_impl->captureTarget = *target;
            auto selected = m_impl->gpu->setPresentationTarget(*target);
            if (!selected)
                return tl::unexpected(selected.error());
        }
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
        auto prepared = m_impl->overlay->prepare();
        if (!prepared)
            return tl::unexpected(prepared.error());
#endif
        auto rendered = m_impl->renderer->record({{}, m_impl->depth, sceneFrame.clearColor},
                                                 sceneFrame.uniforms, sceneFrame.draws);
        if (!rendered)
            return tl::unexpected(rendered.error());
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
        auto overlayRendered = m_impl->overlay->render();
        if (!overlayRendered)
            return tl::unexpected(overlayRendered.error());
#endif
        if (capture) {
            auto copied = m_impl->gpu->copyToSwapchain(m_impl->captureTarget);
            if (!copied)
                return tl::unexpected(copied.error());
        }
        frame.active = false;
        auto submitted = m_impl->gpu->submitFrame();
        if (!submitted)
            return tl::unexpected(submitted.error());
        ++stats.frames;
        if (capture) {
            auto pixels = m_impl->gpu->readback(m_impl->captureTarget);
            if (!pixels)
                return tl::unexpected(pixels.error());
            stats.capture = FrameCapture{size->width, size->height, false, std::move(*pixels)};
#if defined(SOMNIUM_ENABLE_DEBUG_UI)
            stats.capture->debugUi = true;
#endif
            auto released = m_impl->gpu->destroy(m_impl->captureTarget);
            m_impl->captureTarget = {};
            if (!released)
                return tl::unexpected(released.error());
        }
        if (frozen && !captureReady) {
            std::uint32_t requiredFrames = 1;
#if defined(SOMNIUM_ENABLE_ITERATION)
            if (!session.playing())
                requiredFrames = 2;
#endif
            captureReady = ++frozenFrames >= requiredFrames;
            if (captureReady) {
                Log::write("runtime", Log::Level::Info, "CAPTURE_READY steps={}", stats.fixedSteps);
                Log::flush();
            }
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
    if (options.captureFrame && !stats.capture)
        return tl::unexpected(Error{ErrorCode::InvalidState, "Session ended before frame capture"});
#if defined(SOMNIUM_ENABLE_ITERATION)
    if (options.iterationReport) {
        auto canonical = scene::serialize(*m_impl->world, codecs);
        if (!canonical)
            return tl::unexpected(canonical.error());
        report["final"] = std::move(*canonical);
        report["playing"] = session.playing();
        report["dirty"] = editor.dirty();
        report["steps"] = stats.fixedSteps;
        stats.editorReport = report.dump(2) + "\n";
    }
#endif
    return stats;
}
} // namespace somnium::runtime
