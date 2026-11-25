// Rejects malformed or reordered recordings before installing the replay source.
// File names are the same neutral vocabulary as the game's input mapping.
#include <algorithm>
#include <limits>
#include <nlohmann/json.hpp>
#include <somnium/core/input_names.hpp>
#include <somnium/platform/input_replay.hpp>
namespace somnium::platform {
namespace {
using Json = nlohmann::json;
bool fields(const Json& value, std::initializer_list<std::string_view> expected) {
    if (!value.is_object() || value.size() != expected.size())
        return false;
    return std::ranges::all_of(expected, [&](auto field) { return value.contains(field); });
}
template <typename E, std::size_t N>
bool control(const Json& value, const std::array<std::string_view, N>& names, E& output) {
    if (!value.is_string())
        return false;
    const auto text = value.get<std::string>();
    const auto result = core::controlFromName<E>(text, names);
    if (!result || text == "Unknown")
        return false;
    output = *result;
    return true;
}
} // namespace
Result<InputReplay> InputReplay::load(const core::ByteReader& reader, std::string_view path) {
    auto bytes = reader.read(path);
    if (!bytes)
        return tl::unexpected(bytes.error());
    auto result =
        parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    if (!result)
        result.error().message = std::string(path) + ": " + result.error().message;
    return result;
}
Result<InputReplay> InputReplay::parse(std::string_view text) {
    const auto root = Json::parse(text, nullptr, false);
    if (root.is_discarded() || !fields(root, {"version", "events"}) ||
        !root["version"].is_number_integer() || root["version"] != 1 || !root["events"].is_array())
        return tl::unexpected(
            Error{ErrorCode::InvalidArgument, "Replay: expected version 1 events array"});
    InputReplay result;
    std::uint64_t previous = 0;
    for (std::size_t i = 0; i < root["events"].size(); ++i) {
        const auto& value = root["events"][i];
        auto fail = [&] {
            return tl::unexpected(Error{ErrorCode::InvalidArgument,
                                        "Replay.events[" + std::to_string(i) +
                                            "]: invalid fields, control, value or step order"});
        };
        if (!value.is_object() || !value.contains("step") || !value["step"].is_number_unsigned() ||
            !value.contains("kind") || !value["kind"].is_string())
            return fail();
        core::InputEvent event;
        event.step = value["step"].get<std::uint64_t>();
        if (event.step < previous || event.step == std::numeric_limits<std::uint64_t>::max())
            return fail();
        previous = event.step;
        const auto kind = value["kind"].get<std::string>();
        using core::InputEventKind;
        if (kind == "key" || kind == "mouseButton" || kind == "gamepadButton") {
            const bool pad = kind == "gamepadButton";
            if (!(pad ? fields(value, {"step", "kind", "control", "down", "device"})
                      : fields(value, {"step", "kind", "control", "down"})) ||
                !value["down"].is_boolean())
                return fail();
            event.down = value["down"].get<bool>();
            if (kind == "key") {
                event.kind = InputEventKind::Key;
                if (!control(value["control"], core::kKeyNames, event.key))
                    return fail();
            } else if (kind == "mouseButton") {
                event.kind = InputEventKind::MouseButton;
                if (!control(value["control"], core::kMouseButtonNames, event.mouseButton))
                    return fail();
            } else {
                event.kind = InputEventKind::GamepadButton;
                if (!control(value["control"], core::kGamepadButtonNames, event.gamepadButton))
                    return fail();
            }
        } else if (kind == "mouseMotion" || kind == "mouseWheel") {
            if (!fields(value, {"step", "kind", "x", "y"}) || !value["x"].is_number() ||
                !value["y"].is_number())
                return fail();
            event.kind =
                kind == "mouseMotion" ? InputEventKind::MouseMotion : InputEventKind::MouseWheel;
            event.x = value["x"].get<float>();
            event.y = value["y"].get<float>();
        } else if (kind == "gamepadAxis") {
            if (!fields(value, {"step", "kind", "device", "control", "value"}) ||
                !value["value"].is_number_integer() || value["value"] < -32768 ||
                value["value"] > 32767)
                return fail();
            event.kind = InputEventKind::GamepadAxis;
            event.value = value["value"].get<std::int32_t>();
            if (!control(value["control"], core::kGamepadAxisNames, event.gamepadAxis))
                return fail();
        } else if (kind == "gamepadConnected" || kind == "gamepadDisconnected") {
            if (!fields(value, {"step", "kind", "device"}))
                return fail();
            event.kind = kind == "gamepadConnected" ? InputEventKind::GamepadConnected
                                                    : InputEventKind::GamepadDisconnected;
        } else if (kind == "focusLost") {
            if (!fields(value, {"step", "kind"}))
                return fail();
            event.kind = InputEventKind::FocusLost;
        } else
            return fail();
        if (value.contains("device")) {
            if (!value["device"].is_number_unsigned() ||
                value["device"] > std::numeric_limits<std::uint32_t>::max())
                return fail();
            event.device = value["device"].get<std::uint32_t>();
        }
        if (!core::validInputEvent(event))
            return fail();
        result.m_events.push_back(event);
    }
    return result;
}
Result<std::span<const core::InputEvent>> InputReplay::next(std::uint64_t step) {
    if (step != m_nextStep || step == std::numeric_limits<std::uint64_t>::max())
        return tl::unexpected(Error{ErrorCode::InvalidArgument,
                                    "Replay ticks must be consecutive, starting at zero"});
    const auto first = m_cursor;
    while (m_cursor < m_events.size() && m_events[m_cursor].step == step)
        ++m_cursor;
    ++m_nextStep;
    return std::span<const core::InputEvent>(m_events).subspan(first, m_cursor - first);
}
} // namespace somnium::platform
