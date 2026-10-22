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
    std::ofstream report(argv[2]);
    report << nlohmann::json{{"backend", name},
                             {"device", (*device)->info().name},
                             {"sdr_linear", *support},
                             {"srgb_target", srgb},
                             {"acquisition_submission", "passed"}}
                  .dump(2)
           << '\n';
    report.close();
    if (!report)
        return 1;
    std::cout << "COMPOSITION_PROBE_COMPLETE: SDR_LINEAR=" << *support << '\n';
    return 0;
}
