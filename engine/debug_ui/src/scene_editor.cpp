// Treats every registered component, including game codecs, as editable JSON data.
// Quaternion and angle hints change presentation only until a field is actually edited.
#include "scene_editor.hpp"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <somnium/scene/rotation.hpp>
#include <utility>

namespace somnium::debug_ui::detail {
namespace {
using namespace scene;
std::string escaped(std::string_view key) {
    std::string result;
    for (const auto character : key) {
        if (character == '~')
            result += "~0";
        else if (character == '/')
            result += "~1";
        else
            result += character;
    }
    return result;
}
void field(Json& value, const std::string& label, const EditorDescription& description,
           const std::string& path) {
    ImGui::PushID(label.c_str());
    const auto hint = description.fields.find(path);
    bool presented = false;
    if (hint != description.fields.end() && hint->second == FieldPresentation::QuaternionEuler) {
        auto quaternion = readQuaternion(Json{{"value", value}}, "value");
        if (quaternion) {
            auto degrees = eulerDegrees(*quaternion);
            if (ImGui::DragFloat3(label.c_str(), &degrees.x, .5F)) {
                const auto rotation = rotationDegrees(degrees);
                value = {rotation.x, rotation.y, rotation.z, rotation.w};
            }
            ImGui::TextUnformatted("Euler degrees / XYZ");
            presented = true;
        }
    } else if (hint != description.fields.end() &&
               hint->second == FieldPresentation::RadiansDegrees && value.is_number()) {
        auto degrees = glm::degrees(value.get<double>());
        if (ImGui::InputDouble(label.c_str(), &degrees, 0, 0, "%.9g"))
            value = glm::radians(degrees);
        ImGui::TextUnformatted("Degrees");
        presented = true;
    }
    if (!presented) {
        if (value.is_object() || value.is_array()) {
            if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(8);
                for (auto& item : value.items())
                    field(item.value(), item.key(), description, path + "/" + escaped(item.key()));
                ImGui::Unindent(8);
            }
        } else if (value.is_string()) {
            ImGui::InputText(label.c_str(), &value.get_ref<std::string&>());
        } else if (value.is_boolean()) {
            auto flag = value.get<bool>();
            if (ImGui::Checkbox(label.c_str(), &flag))
                value = flag;
        } else if (value.is_number_unsigned()) {
            auto number = value.get<std::uint64_t>();
            if (ImGui::InputScalar(label.c_str(), ImGuiDataType_U64, &number))
                value = number;
        } else if (value.is_number_integer()) {
            auto number = value.get<std::int64_t>();
            if (ImGui::InputScalar(label.c_str(), ImGuiDataType_S64, &number))
                value = number;
        } else if (value.is_number_float()) {
            auto number = value.get<double>();
            if (ImGui::InputDouble(label.c_str(), &number, 0, 0, "%.9g"))
                value = number;
        } else {
            ImGui::Text("%s: null (editable in JSON)", label.c_str());
        }
    }
    ImGui::PopID();
}
} // namespace
void SceneEditor::select(std::string_view component) {
    m_component = component.empty() ? "somnium.Transform" : component;
    m_loadedComponent.clear();
    m_adding = false;
}
void SceneEditor::queue(const scene::Json& command) {
    m_commands.push_back(command.dump());
}
std::vector<std::string> SceneEditor::takeCommands() {
    return std::exchange(m_commands, {});
}
void SceneEditor::draw(const scene::World& world, std::string_view selected,
                       std::uint64_t revision) {
    if (!m_codecs)
        return;
    if (ImGui::Button("Create entity"))
        queue({{"op", "create"}, {"id", ""}, {"name", "Entity"}, {"parent", ""}});
    ImGui::SameLine();
    if (ImGui::Button("Scene settings"))
        select("@scene");
    auto entity = world.find(selected);
    bool settings = m_component == "@scene";
    if (!entity && !settings) {
        ImGui::TextUnformatted("Select an entity");
        return;
    }
    if (entity) {
        ImGui::TextWrapped("ID: %.*s", static_cast<int>(selected.size()), selected.data());
        if (ImGui::Button("Delete subtree"))
            queue({{"op", "delete"}, {"id", selected}});
        ImGui::TextUnformatted("Parent ID (empty = root)");
        ImGui::SetNextItemWidth(-95.F);
        ImGui::InputText("##parent", &m_parent);
        ImGui::SameLine();
        if (ImGui::Button("Reparent"))
            queue({{"op", "reparent"}, {"id", selected}, {"parent", m_parent}});
        ImGui::TextUnformatted("Component");
        ImGui::SetNextItemWidth(-1.F);
        if (ImGui::BeginCombo("##component", m_component.c_str())) {
            for (const auto& [name, codec] : m_codecs->codecs()) {
                if (codec.contains(world, *entity) &&
                    ImGui::Selectable(name.c_str(), name == m_component))
                    select(name);
            }
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(-50.F);
        if (ImGui::BeginCombo("Add", "Choose codec")) {
            for (const auto& [name, codec] : m_codecs->codecs()) {
                if (!codec.contains(world, *entity) && ImGui::Selectable(name.c_str())) {
                    select(name);
                    m_adding = true;
                }
            }
            ImGui::EndCombo();
        }
    }
    settings = m_component == "@scene";
    const auto* codec = settings ? nullptr : m_codecs->find(m_component);
    if (!settings && (!codec || (!m_adding && !codec->contains(world, *entity)))) {
        ImGui::TextUnformatted("Choose an existing component or add one");
        return;
    }
    if (m_loadedEntity != selected || m_loadedComponent != m_component || m_revision != revision) {
        Result<scene::Json> value = scene::Json::object();
        if (settings) {
            scene::Json camera = nullptr;
            if (world.activeCamera != entt::null)
                camera = std::string(*world.id(world.activeCamera));
            value = scene::Json{
                {"settings",
                 {{"ambient", world.settings.ambient}, {"clearColor", world.settings.clearColor}}},
                {"activeCamera", std::move(camera)}};
        } else {
            value = m_adding ? Result<scene::Json>(codec->editor.initial)
                             : codec->serialize(world, *entity);
        }
        if (!value) {
            m_error = value.error().message;
            ImGui::TextWrapped("%s", m_error.c_str());
            return;
        }
        m_draft = std::move(*value);
        m_raw = m_draft.dump(2);
        m_loadedEntity = selected;
        m_loadedComponent = m_component;
        m_revision = revision;
        m_error.clear();
        if (entity) {
            const auto parent = world.registry().get<Hierarchy>(*entity).parent;
            m_parent = parent == entt::null ? "" : std::string(*world.id(parent));
        }
    }
    if (ImGui::Checkbox("Raw JSON", &m_rawMode))
        m_raw = m_draft.dump(2);
    if (m_rawMode)
        ImGui::InputTextMultiline("##draft", &m_raw, ImVec2(-1, 180));
    else {
        const scene::EditorDescription plain;
        ImGui::PushItemWidth(-110.F);
        field(m_draft, "Fields", codec ? codec->editor : plain, "");
        ImGui::PopItemWidth();
    }
    if (ImGui::Button(m_adding ? "Add validated component" : "Apply validated edit")) {
        auto data = m_rawMode ? scene::Json::parse(m_raw, nullptr, false) : m_draft;
        if (data.is_discarded())
            m_error = "Invalid JSON draft";
        else if (settings)
            queue({{"op", "settings"}, {"data", std::move(data)}});
        else
            queue({{"op", m_adding ? "add" : "set"},
                   {"id", selected},
                   {"component", m_component},
                   {"data", std::move(data)}});
    }
    if (!settings && !m_adding && ImGui::Button("Remove component"))
        queue({{"op", "remove"}, {"id", selected}, {"component", m_component}});
    if (!m_error.empty())
        ImGui::TextWrapped("Rejected: %s", m_error.c_str());
    ImGui::TextWrapped("Changes apply at the next fixed step.");
}
} // namespace somnium::debug_ui::detail
