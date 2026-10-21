// Coordinates context ownership, platform events, GPU preparation and overlay drawing.
// Only integration facades cross module boundaries; native backends stay private.
#include "frame_times.hpp"
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
    gpu_imgui::Counters previous{};
    std::string label;
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
Result<void> Overlay::prepare(double frameSeconds) {
    SOMNIUM_PROFILE_SCOPE("debug_ui.prepare");
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
                           ImGuiWindowFlags_NoCollapse;
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
    ImGui::Render();
    return m_impl->gpu->prepare();
}
Result<void> Overlay::render() {
    ImGui::SetCurrentContext(m_impl->context);
    return m_impl->gpu->render();
}
} // namespace somnium::debug_ui
