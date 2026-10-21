// SDL-free facade of the optional official GPU backend.
// Native recording is borrowed from gpu only during preparation and rendering.
#pragma once
#include <cstdint>
#include <memory>
#include <somnium/core/result.hpp>
#include <string>
namespace somnium::gpu {
class Device;
}
namespace somnium::gpu_imgui {
struct Counters {
    std::uint64_t drawCalls, submissions, uploads;
};
class Adapter final {
  public:
    static Result<std::unique_ptr<Adapter>> create(gpu::Device& device);
    ~Adapter();
    Adapter(const Adapter&) = delete;
    Adapter& operator=(const Adapter&) = delete;
    void newFrame();
    [[nodiscard]] Result<void> prepare();
    [[nodiscard]] Result<void> render();
    [[nodiscard]] Counters counters() const;
    [[nodiscard]] std::string label() const;

  private:
    explicit Adapter(gpu::Device& device) : m_device(device) {}
    gpu::Device& m_device;
};
} // namespace somnium::gpu_imgui
