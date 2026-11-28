// Owns the optional diagnostic overlay and sequences its integration facades.
// Public interfaces contain neither SDL nor ImGui types.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <somnium/core/export.hpp>
#include <somnium/core/result.hpp>
#include <string>
#include <string_view>
#include <vector>
namespace somnium {
struct Trs;
struct Camera;
} // namespace somnium
namespace somnium::platform {
class Platform;
}
namespace somnium::gpu {
class Device;
}
namespace somnium::scene {
class World;
class CodecRegistry;
} // namespace somnium::scene
namespace somnium::input {
class Configuration;
struct State;
struct Capture;
} // namespace somnium::input
namespace somnium::debug_ui {
#if defined(SOMNIUM_ENABLE_ITERATION)
struct HistoryStats {
    std::size_t undoCount, redoCount, bytes;
};
struct ModuleStats {
    std::size_t images, fileBytes;
};
#endif
class Overlay final {
  public:
    SOMNIUM_ENGINE_API static Result<std::unique_ptr<Overlay>> create(platform::Platform& platform,
                                                                      gpu::Device& device);
    SOMNIUM_ENGINE_API ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    [[nodiscard]] SOMNIUM_ENGINE_API input::Capture update(double frameSeconds,
                                                           const scene::World& world,
                                                           const input::Configuration& mapping,
                                                           const input::State& inputs, bool replay);
#if defined(SOMNIUM_ENABLE_ITERATION)
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<input::Configuration> iterationMapping();
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void>
    initializeToolCamera(const scene::World& world, const input::Configuration& mapping);
    SOMNIUM_ENGINE_API void updateToolCamera(const input::State& state, double seconds);
    [[nodiscard]] SOMNIUM_ENGINE_API const Trs& toolPose() const;
    [[nodiscard]] SOMNIUM_ENGINE_API const Camera& toolLens() const;
    SOMNIUM_ENGINE_API void configureEditor(const scene::CodecRegistry& codecs);
    SOMNIUM_ENGINE_API void iterationState(bool playing, bool dirty, std::uint64_t revision,
                                           std::string_view error);
    SOMNIUM_ENGINE_API void historyState(HistoryStats state);
    SOMNIUM_ENGINE_API void shaderDiagnostic(std::string_view message);
    SOMNIUM_ENGINE_API void moduleDiagnostic(std::string_view message, ModuleStats stats);
    [[nodiscard]] SOMNIUM_ENGINE_API std::vector<std::string> takeCommands();
    SOMNIUM_ENGINE_API void selectEntity(std::string_view id);
    SOMNIUM_ENGINE_API void selectComponent(std::string_view component);
#endif
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> prepare();
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> render();

  private:
    struct Impl;
    SOMNIUM_ENGINE_API explicit Overlay(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};
} // namespace somnium::debug_ui
