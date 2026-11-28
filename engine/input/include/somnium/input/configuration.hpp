// Compiles named controls into stable indices before simulation starts.
// JSON is private; the fixed-step loop never looks up strings.
#pragma once
#include <limits>
#include <optional>
#include <somnium/core/byte_reader.hpp>
#include <somnium/core/export.hpp>
#include <somnium/core/input_event.hpp>
#include <span>
#include <string>
#include <vector>
namespace somnium::input {
struct ActionId {
    std::uint32_t value = std::numeric_limits<std::uint32_t>::max();
    bool operator==(const ActionId&) const = default;
};
struct AxisId {
    std::uint32_t value = std::numeric_limits<std::uint32_t>::max();
    bool operator==(const AxisId&) const = default;
};
enum class SourceKind : std::uint8_t {
    Key,
    MouseButton,
    GamepadButton,
    GamepadAxis,
    MouseX,
    MouseY,
    WheelX,
    WheelY
};
struct Source {
    SourceKind kind{};
    std::uint16_t control = 0;
    int slot = -1; // -1 means any of the four connected gamepad slots.
};
struct Binding {
    Source source;
    float scale = 1, deadzone = 0;
    std::optional<ActionId> held;
};
struct ActionDefinition {
    std::string name;
    std::vector<Binding> bindings;
};
enum class AxisMode : std::uint8_t { Rate, Delta };
struct AxisDefinition {
    std::string name;
    AxisMode mode{};
    std::vector<Binding> bindings;
};
class Configuration final {
  public:
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<Configuration> parse(std::string_view text);
    [[nodiscard]] SOMNIUM_ENGINE_API static Result<Configuration>
    load(const core::ByteReader& reader, std::string_view path);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<ActionId> action(std::string_view name) const;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<AxisId> axis(std::string_view name) const;
    [[nodiscard]] std::span<const ActionDefinition> actions() const {
        return m_actions;
    }
    [[nodiscard]] std::span<const AxisDefinition> axes() const {
        return m_axes;
    }

  private:
    std::vector<ActionDefinition> m_actions;
    std::vector<AxisDefinition> m_axes;
};
} // namespace somnium::input
