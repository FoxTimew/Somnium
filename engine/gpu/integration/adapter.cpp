// Records the official ImGui renderer on the active GPU frame, in its own integration target.
// Preparation precedes every render pass; rendering loads the already drawn swapchain.
#include <SDL3/SDL_error.h>
#include <algorithm>
#include <imgui_impl_sdlgpu3.h>
#include <somnium/gpu/native.hpp>
#include <somnium/gpu_imgui/adapter.hpp>

namespace somnium::gpu_imgui {
Result<std::unique_ptr<Adapter>> Adapter::create(gpu::Device& device) {
    ImGui_ImplSDLGPU3_InitInfo info{};
    info.Device = gpu::detail::Access::device(device);
    info.ColorTargetFormat = gpu::detail::Access::presentationFormat(device);
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (info.ColorTargetFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
        return tl::unexpected(Error{ErrorCode::InvalidState, "UI requires an attached swapchain"});
    if (!ImGui_ImplSDLGPU3_Init(&info))
        return tl::unexpected(Error{ErrorCode::Gpu, SDL_GetError()});
    return std::unique_ptr<Adapter>(new Adapter(device));
}
Adapter::~Adapter() {
    ImGui_ImplSDLGPU3_Shutdown();
}
void Adapter::newFrame() {
    ImGui_ImplSDLGPU3_NewFrame();
}
Result<void> Adapter::prepare() {
    auto frame = gpu::detail::Access::frame(m_device);
    if (!frame)
        return tl::unexpected(frame.error());
    auto* data = ImGui::GetDrawData();
    // A resize can arrive after acquisition. The window's new pixel size must not
    // produce a viewport larger than this frame's already acquired texture.
    if (data->DisplaySize.x > 0 && data->DisplaySize.y > 0) {
        data->FramebufferScale = {static_cast<float>(frame->extent.width) / data->DisplaySize.x,
                                  static_cast<float>(frame->extent.height) / data->DisplaySize.y};
    }
    std::uint64_t textureUploads = 0;
    const bool drawable =
        data->DisplaySize.x > 0 && data->DisplaySize.y > 0 && data->TotalVtxCount > 0;
    if (drawable && data->Textures) {
        for (const auto* texture : *data->Textures)
            if (texture->Status == ImTextureStatus_WantCreate ||
                texture->Status == ImTextureStatus_WantUpdates)
                ++textureUploads;
    }
    ImGui_ImplSDLGPU3_PrepareDrawData(data, frame->commands);
    gpu::detail::Access::count(
        m_device, {.uploads = drawable ? 2 + textureUploads : 0, .submissions = textureUploads});
    return {};
}
Result<void> Adapter::render() {
    auto frame = gpu::detail::Access::frame(m_device);
    if (!frame)
        return tl::unexpected(frame.error());
    SDL_GPUColorTargetInfo color{};
    color.texture = frame->color;
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_PushGPUDebugGroup(frame->commands, "Somnium debug overlay");
    auto* pass = SDL_BeginGPURenderPass(frame->commands, &color, 1, nullptr);
    if (!pass) {
        SDL_PopGPUDebugGroup(frame->commands);
        return tl::unexpected(Error{ErrorCode::Gpu, SDL_GetError()});
    }
    auto* data = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_RenderDrawData(data, frame->commands, pass);
    SDL_EndGPURenderPass(pass);
    SDL_PopGPUDebugGroup(frame->commands);
    std::uint64_t draws = 0;
    const auto scale = data->FramebufferScale;
    const auto offset = data->DisplayPos;
    const float width = data->DisplaySize.x * scale.x, height = data->DisplaySize.y * scale.y;
    for (const auto* list : data->CmdLists) {
        for (const auto& command : list->CmdBuffer) {
            const auto clip = command.ClipRect;
            if (!command.UserCallback &&
                std::min((clip.z - offset.x) * scale.x, width) >
                    std::max((clip.x - offset.x) * scale.x, 0.0F) &&
                std::min((clip.w - offset.y) * scale.y, height) >
                    std::max((clip.y - offset.y) * scale.y, 0.0F))
                ++draws;
        }
    }
    gpu::detail::Access::count(m_device, {.draws = draws});
    return {};
}
Counters Adapter::counters() const {
    const auto stats = m_device.statistics();
    return {stats.drawCalls, stats.submissions, stats.uploads};
}
std::string Adapter::label() const {
    return m_device.info().driver + " / " + m_device.info().name;
}
} // namespace somnium::gpu_imgui
