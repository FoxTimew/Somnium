// Validates a whole mapping before publishing any compiled control identifiers.
// Unknown fields and overlapping physical sources fail with a JSON location.
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <somnium/core/input_names.hpp>
#include <somnium/input/configuration.hpp>
namespace somnium::input {
namespace {
using Json = nlohmann::json;
auto error(std::string_view path, std::string_view message) {
    return tl::unexpected(
        Error{ErrorCode::InvalidArgument, std::string(path) + ": " + std::string(message)});
}
bool fields(const Json& value, std::initializer_list<std::string_view> required,
            std::initializer_list<std::string_view> optional = {}) {
    if (!value.is_object())
        return false;
    for (auto field : required)
        if (!value.contains(field))
            return false;
    for (const auto& [key, ignored] : value.items()) {
        (void)ignored;
        if (std::ranges::find(required, key) == required.end() &&
            std::ranges::find(optional, key) == optional.end())
            return false;
    }
    return true;
}
Result<std::string> name(const Json& value, std::string_view path) {
    if (!value.is_string())
        return error(path, "expected string");
    auto result = value.get<std::string>();
    if (result.empty() || result.find('\0') != std::string::npos)
        return error(path, "empty or invalid name");
    return result;
}
Result<float> number(const Json& value, std::string_view path) {
    if (!value.is_number())
        return error(path, "expected finite number");
    const auto result = value.get<float>();
    if (!std::isfinite(result))
        return error(path, "expected finite number");
    return result;
}
template <typename E, std::size_t N>
Result<std::uint16_t> control(const Json& value, const std::array<std::string_view, N>& names,
                              std::string_view path) {
    auto text = name(value, path);
    if (!text)
        return tl::unexpected(text.error());
    auto found = core::controlFromName<E>(*text, names);
    if (!found || *text == "Unknown")
        return error(path, "unknown physical control");
    return static_cast<std::uint16_t>(*found);
}
Result<Binding> binding(const Json& value, bool action, AxisMode mode, const Configuration& config,
                        std::string_view path) {
    if (!fields(value, {"source"}, {"control", "slot", "scale", "invert", "deadzone", "held"}))
        return error(path, "unknown or missing binding field");
    auto source = name(value["source"], path);
    if (!source)
        return tl::unexpected(source.error());
    Binding result;
    if (*source == "key")
        result.source.kind = SourceKind::Key;
    else if (*source == "mouseButton")
        result.source.kind = SourceKind::MouseButton;
    else if (*source == "gamepadButton")
        result.source.kind = SourceKind::GamepadButton;
    else if (*source == "gamepadAxis")
        result.source.kind = SourceKind::GamepadAxis;
    else if (*source == "mouseX")
        result.source.kind = SourceKind::MouseX;
    else if (*source == "mouseY")
        result.source.kind = SourceKind::MouseY;
    else if (*source == "wheelX")
        result.source.kind = SourceKind::WheelX;
    else if (*source == "wheelY")
        result.source.kind = SourceKind::WheelY;
    else
        return error(path, "unknown source");
    const auto kind = result.source.kind;
    const bool delta = kind >= SourceKind::MouseX;
    const bool pad = kind == SourceKind::GamepadAxis || kind == SourceKind::GamepadButton;
    if (action && kind > SourceKind::GamepadButton)
        return error(path, "action requires a button");
    if (!action && delta != (mode == AxisMode::Delta))
        return error(path, "source incompatible with axis mode");
    if (delta == value.contains("control"))
        return error(path, "control required only for named buttons/axes");
    if (!delta) {
        Result<std::uint16_t> code = error(path, "invalid control");
        switch (kind) {
        case SourceKind::Key:
            code = control<core::Key>(value["control"], core::kKeyNames, path);
            break;
        case SourceKind::MouseButton:
            code = control<core::MouseButton>(value["control"], core::kMouseButtonNames, path);
            break;
        case SourceKind::GamepadButton:
            code = control<core::GamepadButton>(value["control"], core::kGamepadButtonNames, path);
            break;
        case SourceKind::GamepadAxis:
            code = control<core::GamepadAxis>(value["control"], core::kGamepadAxisNames, path);
            break;
        default:
            break;
        }
        if (!code)
            return tl::unexpected(code.error());
        result.source.control = *code;
    }
    if (value.contains("slot")) {
        const auto& slot = value["slot"];
        if (!pad)
            return error(path, "slot is only valid for gamepads");
        if (slot.is_number_integer() && slot >= 0 && slot <= 3)
            result.source.slot = slot.get<int>();
        else if (!(slot.is_string() && slot == "any"))
            return error(path, "slot must be any or 0..3");
    }
    if (action && (value.contains("scale") || value.contains("invert") ||
                   value.contains("deadzone") || value.contains("held")))
        return error(path, "axis options on an action");
    if (value.contains("scale")) {
        auto scale = number(value["scale"], path);
        if (!scale)
            return tl::unexpected(scale.error());
        result.scale = *scale;
    }
    if (value.contains("invert")) {
        if (!value["invert"].is_boolean())
            return error(path, "invert must be boolean");
        if (value["invert"].get<bool>())
            result.scale = -result.scale;
    }
    if (value.contains("deadzone")) {
        auto deadzone = number(value["deadzone"], path);
        if (!deadzone)
            return tl::unexpected(deadzone.error());
        if (kind != SourceKind::GamepadAxis || *deadzone < 0 || *deadzone >= 1)
            return error(path, "deadzone requires a gamepad axis and 0 <= value < 1");
        result.deadzone = *deadzone;
    }
    if (value.contains("held")) {
        if (!delta)
            return error(path, "held condition is only valid for delta axes");
        auto held = name(value["held"], path);
        if (!held)
            return tl::unexpected(held.error());
        auto id = config.action(*held);
        if (!id)
            return error(path, "unknown held action");
        result.held = *id;
    }
    return result;
}
} // namespace
Result<ActionId> Configuration::action(std::string_view value) const {
    for (std::size_t i = 0; i < m_actions.size(); ++i)
        if (m_actions[i].name == value)
            return ActionId{static_cast<std::uint32_t>(i)};
    return error(value, "unknown action");
}
Result<AxisId> Configuration::axis(std::string_view value) const {
    for (std::size_t i = 0; i < m_axes.size(); ++i)
        if (m_axes[i].name == value)
            return AxisId{static_cast<std::uint32_t>(i)};
    return error(value, "unknown axis");
}
Result<Configuration> Configuration::load(const core::ByteReader& reader, std::string_view path) {
    auto bytes = reader.read(path);
    if (!bytes)
        return tl::unexpected(bytes.error());
    auto result =
        parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    if (!result)
        result.error().message = std::string(path) + ": " + result.error().message;
    return result;
}
Result<Configuration> Configuration::parse(std::string_view text) {
    auto root = Json::parse(text, nullptr, false);
    if (root.is_discarded() || !fields(root, {"version", "actions", "axes"}) ||
        !root["version"].is_number_integer() || root["version"] != 1 ||
        !root["actions"].is_array() || !root["axes"].is_array())
        return error("mapping", "expected version 1 actions/axes arrays");
    if (root["actions"].size() + root["axes"].size() > 4096)
        return error("mapping", "too many controls");
    Configuration result;
    std::set<std::string> names;
    for (auto category : {"actions", "axes"}) {
        const bool action = std::string_view(category) == "actions";
        for (std::size_t i = 0; i < root[category].size(); ++i) {
            const auto path = std::string(category) + "[" + std::to_string(i) + "]";
            const auto& value = root[category][i];
            if (!(action ? fields(value, {"name", "bindings"})
                         : fields(value, {"name", "bindings", "mode"})) ||
                !value["bindings"].is_array() || value["bindings"].empty())
                return error(path, "expected named nonempty bindings");
            auto label = name(value["name"], path);
            if (!label)
                return tl::unexpected(label.error());
            if (!names.insert(*label).second)
                return error(path, "duplicate control name");
            if (action)
                result.m_actions.push_back({*label, {}});
            else {
                if (value["mode"] != "rate" && value["mode"] != "delta")
                    return error(path, "mode must be rate or delta");
                result.m_axes.push_back(
                    {*label, value["mode"] == "rate" ? AxisMode::Rate : AxisMode::Delta, {}});
            }
        }
    }
    std::vector<std::pair<Source, std::string>> used;
    for (auto category : {"actions", "axes"}) {
        const bool action = std::string_view(category) == "actions";
        for (std::size_t i = 0; i < root[category].size(); ++i) {
            const auto& bindings = root[category][i]["bindings"];
            auto& output = action ? result.m_actions[i].bindings : result.m_axes[i].bindings;
            for (std::size_t j = 0; j < bindings.size(); ++j) {
                const auto path = std::string(category) + "[" + std::to_string(i) + "].bindings[" +
                                  std::to_string(j) + "]";
                auto compiled =
                    binding(bindings[j], action, action ? AxisMode::Rate : result.m_axes[i].mode,
                            result, path);
                if (!compiled)
                    return tl::unexpected(compiled.error());
                const auto source = compiled->source;
                for (const auto& [other, location] : used)
                    if (source.kind == other.kind && source.control == other.control &&
                        (source.slot == other.slot || source.slot == -1 || other.slot == -1))
                        return error(path, "physical source conflicts with " + location);
                used.emplace_back(source, path);
                output.push_back(*compiled);
            }
        }
    }
    return result;
}
} // namespace somnium::input
