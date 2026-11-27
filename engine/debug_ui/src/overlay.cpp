// Coordinates context ownership, platform events, GPU preparation and overlay drawing.
// Only integration facades cross module boundaries; native backends stay private.
#include "frame_times.hpp"
#include "inspector.hpp"
#if defined(SOMNIUM_ENABLE_ITERATION)
#include "editor_camera.hpp"
#endif
#include <imgui.h>
#include <somnium/core/profile.hpp>
#include <somnium/debug_ui/overlay.hpp>
#include <somnium/gpu_imgui/adapter.hpp>
#include <somnium/platform_imgui/adapter.hpp>
#include <utility>

namespace somnium::debug_ui {
struct Overlay::Impl {
    ImGuiContext* context = nullptr;
    std::unique_ptr<platform_imgui::Adapter> platform;
    std::unique_ptr<gpu_imgui::Adapter> gpu;
    detail::FrameTimes timing;
    detail::Inspector inspector;
    gpu_imgui::Counters previous{};
    std::string label;
#if defined(SOMNIUM_ENABLE_ITERATION)
    detail::EditorCamera camera;
    bool playing = true;
    std::optional<bool> playRequest;
    std::string iterationError;
#endif
    ~Impl() {
        if (context) {
            ImGui::SetCurrentContext(context);
            gpu.reset();
            platform.reset();
            ImGui::DestroyContext(context);
        }
    }
};
Overlay::Overlay(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
Overlay::~Overlay() = default;
Result<std::unique_ptr<Overlay>> Overlay::create(platform::Platform& platform,
                                                 gpu::Device& device) {
    if (ImGui::GetCurrentContext())
        return tl::unexpected(Error{ErrorCode::InvalidState, "UI context already owned"});
    auto impl = std::make_unique<Impl>();
    IMGUI_CHECKVERSION();
    impl->context = ImGui::CreateContext();
    if (!impl->context)
        return tl::unexpected(Error{ErrorCode::InvalidState, "Cannot create UI context"});
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().FontScaleMain = 1.25F;
    auto platformBackend = platform_imgui::Adapter::create(platform);
    if (!platformBackend)
        return tl::unexpected(platformBackend.error());
    impl->platform = std::move(*platformBackend);
    auto gpuBackend = gpu_imgui::Adapter::create(device);
    if (!gpuBackend)
        return tl::unexpected(gpuBackend.error());
    impl->gpu = std::move(*gpuBackend);
    impl->label = impl->gpu->label();
    impl->previous = impl->gpu->counters();
    return std::unique_ptr<Overlay>(new Overlay(std::move(impl)));
}
input::Capture Overlay::update(double frameSeconds, const scene::World& world,
                               const input::Configuration& mapping, const input::State& inputs,
                               bool replay) {
    SOMNIUM_PROFILE_SCOPE("debug_ui.update");
    ImGui::SetCurrentContext(m_impl->context);
    m_impl->gpu->newFrame();
    m_impl->platform->newFrame();
    ImGui::NewFrame();
    m_impl->timing.sample(frameSeconds);
    const auto counters = m_impl->gpu->counters();
    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92F);
    constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("Somnium diagnostics", nullptr, flags)) {
        ImGui::TextUnformatted(m_impl->label.c_str());
        ImGui::Separator();
        ImGui::Text("%.1f FPS  /  %.3f ms", m_impl->timing.fps(), m_impl->timing.milliseconds());
        ImGui::Text("Draw calls: %llu", static_cast<unsigned long long>(
                                            counters.drawCalls - m_impl->previous.drawCalls));
        ImGui::Text("Submissions: %llu", static_cast<unsigned long long>(
                                             counters.submissions - m_impl->previous.submissions));
        ImGui::Text("Uploads: %llu",
                    static_cast<unsigned long long>(counters.uploads - m_impl->previous.uploads));
        ImGui::TextUnformatted("Previous frame, including UI / VSYNC / 2 frames in flight");
    }
    ImGui::End();
    m_impl->previous = counters;
#if defined(SOMNIUM_ENABLE_ITERATION)
    ImGui::SetNextWindowPos(ImVec2(16, 190), ImGuiCond_Always);
    if (ImGui::Begin("Iteration", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(m_impl->playing ? "Play / simulation running"
                                               : "Edit / simulation stopped");
        if (ImGui::Button(m_impl->playing ? "Stop" : "Play"))
            m_impl->playRequest = !m_impl->playing;
        if (!m_impl->playing)
            ImGui::TextUnformatted("Tool camera: RMB look, WASD + Q/E, wheel");
        if (!m_impl->iterationError.empty())
            ImGui::TextWrapped("Rejected: %s", m_impl->iterationError.c_str());
    }
    ImGui::End();
#endif
    bool editing = false;
#if defined(SOMNIUM_ENABLE_ITERATION)
    editing = !m_impl->playing;
#endif
    m_impl->inspector.draw(world, mapping, inputs, replay, editing);
    ImGui::Render();
    const auto& io = ImGui::GetIO();
    return replay ? input::Capture{} : input::Capture{io.WantCaptureKeyboard, io.WantCaptureMouse};
}
#if defined(SOMNIUM_ENABLE_ITERATION)
Result<input::Configuration> Overlay::iterationMapping() {
    return detail::EditorCamera::configuration();
}
Result<void> Overlay::initializeToolCamera(const scene::World& world,
                                           const input::Configuration& mapping) {
    return m_impl->camera.initialize(world, mapping);
}
void Overlay::updateToolCamera(const input::State& state, double seconds) {
    m_impl->camera.update(state, seconds);
}
const Trs& Overlay::toolPose() const {
    return m_impl->camera.pose();
}
const Camera& Overlay::toolLens() const {
    return m_impl->camera.lens();
}
void Overlay::iterationState(bool playing, std::string_view error) {
    m_impl->playing = playing;
    m_impl->iterationError = error;
}
std::optional<bool> Overlay::takePlayRequest() {
    return std::exchange(m_impl->playRequest, std::nullopt);
}
void Overlay::applyEdits(scene::World& world) {
    m_impl->inspector.apply(world);
}
#endif
Result<void> Overlay::prepare() {
    SOMNIUM_PROFILE_SCOPE("debug_ui.prepare");
    ImGui::SetCurrentContext(m_impl->context);
    return m_impl->gpu->prepare();
}
Result<void> Overlay::render() {
    ImGui::SetCurrentContext(m_impl->context);
    return m_impl->gpu->render();
}
} // namespace somnium::debug_ui
