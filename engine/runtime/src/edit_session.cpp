// Keeps failures recoverable and never publishes a partially decoded edit or reload.
// Replay uses the same commands as the UI; it only adds deterministic failure expectations.
#include "edit_session.hpp"
#include "scene_editor.hpp"

namespace somnium::runtime {
namespace {
auto invalid(std::string message) {
    return tl::unexpected(Error{ErrorCode::InvalidArgument, std::move(message)});
}
} // namespace
Result<void> EditSession::initialize() {
    auto source = m_files.read(m_path);
    if (!source)
        return tl::unexpected(source.error());
    auto canonical = scene::serialize(*m_world, m_codecs);
    if (!canonical)
        return tl::unexpected(canonical.error());
    auto observed = scene::deserialize(*source, m_codecs, m_assets);
    if (!observed)
        return tl::unexpected(observed.error());
    auto observedCanonical = scene::serialize(**observed, m_codecs);
    if (!observedCanonical)
        return tl::unexpected(observedCanonical.error());
    if (*observedCanonical != *canonical)
        return tl::unexpected(Error{ErrorCode::InvalidState,
                                    "Source changed during scene initialization; reload required"});
    m_source = std::move(*source);
    m_saved = std::move(*canonical);
    m_current = m_saved;
    return {};
}
Result<EditEffect> EditSession::execute(const scene::Json& command, std::uint64_t tick) {
    auto result = apply(command, tick);
    m_error = result ? std::string{} : result.error().message;
    return result;
}
Result<EditEffect> EditSession::apply(const scene::Json& command, std::uint64_t tick) {
    using namespace scene;
    auto operation = readString(command, "op");
    if (!operation)
        return tl::unexpected(operation.error());
    if (*operation == "play" || *operation == "stop") {
        auto fields = requireFields(command, {"op"});
        if (!fields)
            return tl::unexpected(fields.error());
        auto changed = *operation == "play" ? m_modes.play(tick) : m_modes.stop();
        if (!changed)
            return tl::unexpected(changed.error());
        ++m_revision;
        return EditEffect{};
    }
    if (*operation == "select") {
        auto fields = requireFields(command, {"op", "id", "component"});
        if (!fields)
            return tl::unexpected(fields.error());
        auto id = readString(command, "id"), component = readString(command, "component");
        if (!id)
            return tl::unexpected(id.error());
        if (!component)
            return tl::unexpected(component.error());
        auto entity = m_world->find(*id);
        if (!entity)
            return tl::unexpected(entity.error());
        const auto* codec = m_codecs.find(*component);
        if (!component->empty() && (!codec || !codec->contains(*m_world, *entity)))
            return invalid("Selected component is absent");
        return EditEffect{std::move(*id), std::move(*component)};
    }
    if (m_modes.playing())
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Editing and saving are refused in Play; Stop first"});
    if (*operation == "save") {
        auto fields = requireFields(command, {"op"});
        if (!fields)
            return tl::unexpected(fields.error());
        auto saved = m_files.replace(m_path, {.expected = m_source, .contents = m_current});
        if (!saved)
            return tl::unexpected(saved.error());
        m_source = m_saved = m_current;
        m_dirty = false;
        return EditEffect{};
    }
    if (*operation == "reload") {
        auto fields = requireFields(command, {"op", "discard"});
        if (!fields)
            return tl::unexpected(fields.error());
        if (!command["discard"].is_boolean())
            return invalid("discard must be boolean");
        if (m_dirty && !command["discard"].get<bool>())
            return tl::unexpected(Error{ErrorCode::InvalidState,
                                        "Unsaved edits: explicit discard required to reload"});
        auto source = m_files.read(m_path);
        if (!source)
            return tl::unexpected(source.error());
        auto candidate = deserialize(*source, m_codecs, m_assets);
        if (!candidate)
            return tl::unexpected(candidate.error());
        auto canonical = serialize(**candidate, m_codecs);
        if (!canonical)
            return tl::unexpected(canonical.error());
        m_world = std::move(*candidate);
        m_source = std::move(*source);
        m_current = m_saved = std::move(*canonical);
        m_dirty = false;
        ++m_revision;
        return EditEffect{};
    }
    auto edited = editWorld(*m_world, m_codecs, command, m_assets);
    if (!edited)
        return tl::unexpected(edited.error());
    auto canonical = serialize(*edited->world, m_codecs);
    if (!canonical)
        return tl::unexpected(canonical.error());
    m_world = std::move(edited->world);
    m_current = std::move(*canonical);
    m_dirty = m_current != m_saved;
    ++m_revision;
    std::string component;
    if (command.contains("component"))
        component = command["component"].get<std::string>();
    return EditEffect{std::move(edited->selected), std::move(component)};
}
Result<std::vector<EditorReplayEntry>> parseEditorReplay(std::string_view text) {
    using namespace scene;
    const auto document = Json::parse(text.begin(), text.end(), nullptr, false);
    auto fields = requireFields(document, {"version", "commands"});
    if (!fields)
        return tl::unexpected(fields.error());
    if (document["version"] != 1 || !document["version"].is_number_integer() ||
        !document["commands"].is_array())
        return invalid("Editor replay requires version 1 and commands array");
    std::vector<EditorReplayEntry> entries;
    for (const auto& entry : document["commands"]) {
        fields = entry.contains("errorContains")
                     ? requireFields(entry, {"step", "command", "errorContains"})
                     : requireFields(entry, {"step", "command"});
        if (!fields)
            return tl::unexpected(fields.error());
        if (!entry["step"].is_number_unsigned() || !entry["command"].is_object())
            return invalid("Editor replay requires unsigned step and command object");
        const auto step = entry["step"].get<std::uint64_t>();
        if (!entries.empty() && step < entries.back().step)
            return invalid("Editor replay steps must be ordered");
        std::string error;
        if (entry.contains("errorContains")) {
            auto expected = readString(entry, "errorContains");
            if (!expected || expected->empty())
                return invalid("errorContains must be a nonempty string");
            error = std::move(*expected);
        }
        entries.push_back({step, entry["command"], std::move(error)});
    }
    return entries;
}
} // namespace somnium::runtime
