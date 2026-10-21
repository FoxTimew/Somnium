// Adapts platform's private event stream to the unmodified official ImGui backend.
// Native events never reach debug_ui, runtime or gpu.
#include <SDL3/SDL_error.h>
#include <imgui_impl_sdl3.h>
#include <somnium/platform/events.hpp>
#include <somnium/platform/native.hpp>
#include <somnium/platform_imgui/adapter.hpp>

namespace somnium::platform_imgui {
Result<std::unique_ptr<Adapter>> Adapter::create(platform::Platform& platform) {
    auto* window = platform::detail::Access::window(platform);
    if (!window || !ImGui_ImplSDL3_InitForSDLGPU(window))
        return tl::unexpected(Error{ErrorCode::Platform, SDL_GetError()});
    auto result = std::unique_ptr<Adapter>(new Adapter(platform));
    platform::detail::EventAccess::setObserver(
        platform, nullptr,
        [](void*, const SDL_Event& event) { ImGui_ImplSDL3_ProcessEvent(&event); });
    return result;
}
Adapter::~Adapter() {
    platform::detail::EventAccess::setObserver(m_platform, nullptr, nullptr);
    ImGui_ImplSDL3_Shutdown();
}
void Adapter::newFrame() {
    ImGui_ImplSDL3_NewFrame();
}
} // namespace somnium::platform_imgui
