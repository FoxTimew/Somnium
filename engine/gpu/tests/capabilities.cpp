// Queries the actual presentation surface, then acquires/submits the supported composition.
// This probe does not decide color policy from the backend's name.
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <somnium/gpu/device.hpp>
#include <somnium/platform/platform.hpp>

int main(int argc, char** argv) {
    using namespace somnium;
    if (argc != 3)
        return 2;
    const std::string_view name(argv[1]);
    const auto backend = name == "vulkan"       ? gpu::Backend::Vulkan
                         : name == "direct3d12" ? gpu::Backend::D3D12
                                                : gpu::Backend::Metal;
    auto window = platform::Platform::create({"Somnium composition probe", 128, 128});
    auto device = gpu::Device::create({backend, true});
    if (!window || !device)
        return 1;
    if (!(*device)->attachWindow(**window))
        return 1;
    auto support = (*device)->supportsComposition(gpu::Composition::SdrLinear);
    if (!support)
        return 1;
    (*device).reset();
    device = gpu::Device::create({backend, true});
    if (!device)
        return 1;
    auto format = (*device)->attachWindow(**window, *support ? gpu::Composition::SdrLinear
                                                             : gpu::Composition::Sdr);
    if (!format || !(*device)->beginFrame())
        return 1;
    auto extent = (*device)->acquireSwapchain();
    if (!extent || !extent->drawable())
        return 1;
    if (!(*device)->recordPass({{}, {}, {0.25F, 0.25F, 0.25F, 1}}, {}) || !(*device)->submitFrame())
        return 1;
    const bool srgb =
        *format == gpu::ColorFormat::Rgba8Srgb || *format == gpu::ColorFormat::Bgra8Srgb;
    if (srgb != *support)
        return 1;
    gpu::TextureKind kind{};
    switch (*format) {
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
    const std::array clear{0.25F, 0.5F, 0.75F, 1.F};
    auto target = (*device)->createTexture({kind, extent->width, extent->height, clear});
    auto small = (*device)->createTexture({kind, 1, 1});
    auto wrongFormat = (*device)->createTexture(
        {srgb ? gpu::TextureKind::ColorRgba8 : gpu::TextureKind::ColorRgba8Srgb, extent->width,
         extent->height});
    if (!target || !small || !wrongFormat || !(*device)->beginFrame())
        return 1;
    auto acquired = (*device)->acquireSwapchain();
    if (!acquired || !acquired->drawable() || (*device)->setPresentationTarget(*small) ||
        (*device)->copyToSwapchain(*wrongFormat) || !(*device)->setPresentationTarget(*target) ||
        !(*device)->recordPass({{}, {}, clear}, {}) || !(*device)->copyToSwapchain(*target) ||
        !(*device)->submitFrame())
        return 1;
    auto pixels = (*device)->readback(*target);
    if (!pixels || pixels->size() != std::size_t(extent->width) * extent->height * 4)
        return 1;
    // Selected target handles retire immediately, even while their native texture is in use.
    if (!(*device)->beginFrame())
        return 1;
    acquired = (*device)->acquireSwapchain();
    if (!acquired || !acquired->drawable() || !(*device)->setPresentationTarget(*target) ||
        !(*device)->recordPass({{}, {}, clear}, {}) || !(*device)->copyToSwapchain(*target) ||
        !(*device)->destroy(*target))
        return 1;
    const auto stale = (*device)->recordPass({{}, {}, clear}, {});
    if (stale || stale.error().code != ErrorCode::InvalidHandle ||
        (*device)->copyToSwapchain(*target) || (*device)->statistics().pendingReleases != 1 ||
        !(*device)->finishFrame() || (*device)->statistics().pendingReleases != 0)
        return 1;
    std::ofstream report(argv[2]);
    report << nlohmann::json{{"backend", name},
                             {"device", (*device)->info().name},
                             {"sdr_linear", *support},
                             {"srgb_target", srgb},
                             {"composed_target_copy_readback_retirement", "passed"},
                             {"acquisition_submission", "passed"}}
                  .dump(2)
           << '\n';
    report.close();
    if (!report)
        return 1;
    std::cout << "COMPOSITION_PROBE_COMPLETE: SDR_LINEAR=" << *support << '\n';
    return 0;
}
