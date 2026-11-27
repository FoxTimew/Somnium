// Keeps all ImGui interaction in debug_ui and all scene mutations in World.
// Replay disables UI interaction but retains the same visible inspection panels.
#include "inspector.hpp"
#include <algorithm>
#include <imgui.h>
#include <somnium/scene/rotation.hpp>
namespace somnium::debug_ui::detail {
void Inspector::hierarchy(const scene::World& world) {
    const auto& registry = world.registry();
    m_ancestors.clear();
    m_ancestors.reserve(world.size());
    for (auto entity : world.traverse()) {
        const auto& hierarchy = registry.get<Hierarchy>(entity);
        while (!m_ancestors.empty() && m_ancestors.back().entity != hierarchy.parent)
            m_ancestors.pop_back();
        const bool visible = m_ancestors.empty() || m_ancestors.back().visibleChildren;
        bool open = false;
        if (visible) {
            const auto& id = registry.get<PersistentId>(entity).value;
            const auto& name = registry.get<Name>(entity).value;
            auto flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_OpenOnArrow |
                         ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
            if (hierarchy.firstChild == entt::null)
                flags |= ImGuiTreeNodeFlags_Leaf;
            if (id == m_selected)
                flags |= ImGuiTreeNodeFlags_Selected;
            const float indent = static_cast<float>(m_ancestors.size()) * 16.F;
            if (indent > 0)
                ImGui::Indent(indent);
            open = ImGui::TreeNodeEx(id.c_str(), flags, "%s",
                                     name.empty() ? id.c_str() : name.c_str());
            if (id == m_selected && m_scrollToSelection)
                ImGui::SetScrollHereY(.5F);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                m_selected = id;
#if defined(SOMNIUM_ENABLE_ITERATION)
                m_editor.select({});
#endif
            }
            if (indent > 0)
                ImGui::Unindent(indent);
        }
        m_ancestors.push_back({entity, visible && open});
    }
    m_ancestors.clear();
    m_scrollToSelection = false;
}
void Inspector::transform(const scene::World& world) {
    auto entity = world.find(m_selected);
    if (!entity) {
        ImGui::TextUnformatted("Select an entity");
        return;
    }
    const auto& local = world.registry().get<Transform>(*entity).current;
    const auto degrees = scene::eulerDegrees(local.rotation);
    ImGui::TextWrapped("ID: %s", m_selected.c_str());
    ImGui::Text("Position %.3f %.3f %.3f", local.position.x, local.position.y, local.position.z);
    ImGui::Text("Rotation %.2f %.2f %.2f", degrees.x, degrees.y, degrees.z);
    ImGui::Text("Scale %.3f %.3f %.3f", local.scale.x, local.scale.y, local.scale.z);
    ImGui::TextUnformatted("Read only / use the iteration preset to edit");
}
void Inspector::draw(const scene::World& world, const input::Configuration& mapping,
                     const input::State& inputs, bool replay, bool editing) {
    if (m_selected.empty() && world.registry().valid(world.activeCamera))
        m_selected = world.registry().get<PersistentId>(world.activeCamera).value;
    const auto display = ImGui::GetIO().DisplaySize;
    const float width = std::min(350.F, display.x);
    ImGui::SetNextWindowPos(ImVec2(std::max(0.F, display.x - width - 12.F), 12.F),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, std::max(240.F, display.y - 24.F)), ImGuiCond_Always);
    const auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
                       (replay ? ImGuiWindowFlags_NoInputs : 0);
    if (ImGui::Begin("Scene inspector", nullptr, flags)) {
        if (replay)
            ImGui::TextUnformatted("Scripted replay / read only");
        ImGui::BeginDisabled(replay);
        const auto hierarchyHeight = editing ? 140.F : 180.F;
        if (ImGui::BeginChild("Hierarchy", ImVec2(0, std::min(hierarchyHeight, display.y * .25F)),
                              ImGuiChildFlags_Borders))
            hierarchy(world);
        ImGui::EndChild();
#if defined(SOMNIUM_ENABLE_ITERATION)
        if (editing)
            m_editor.draw(world, m_selected, m_revision);
        else
            transform(world);
#else
        (void)editing;
        transform(world);
#endif
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::Text("Inputs / step %llu", static_cast<unsigned long long>(inputs.step));
        for (std::size_t i = 0; i < mapping.actions().size(); ++i) {
            const auto& value = inputs.action({static_cast<std::uint32_t>(i)});
            ImGui::Text("[%zu] %s: %s%s%s", i, mapping.actions()[i].name.c_str(),
                        value.held ? "held " : "- ", value.pressed ? "pressed " : "",
                        value.released ? "released" : "");
        }
        for (std::size_t i = 0; i < mapping.axes().size(); ++i)
            ImGui::Text("[%zu] %s: %.3f", i, mapping.axes()[i].name.c_str(),
                        inputs.axis({static_cast<std::uint32_t>(i)}));
        ImGui::Text("Transitions: %zu", inputs.transitions.size());
        for (const auto& transition : inputs.transitions)
            ImGui::Text("[%u] %s", transition.action.value,
                        transition.down ? "pressed" : "released");
    }
    ImGui::End();
}
} // namespace somnium::debug_ui::detail
