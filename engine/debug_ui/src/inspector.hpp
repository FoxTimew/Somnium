// Displays hierarchy and inputs, with optional generic codec drafts in iteration.
// Persistent IDs keep selection stable across registry storage changes.
#pragma once
#if defined(SOMNIUM_ENABLE_ITERATION)
#include "scene_editor.hpp"
#else
#include <somnium/scene/world.hpp>
#endif
#include <memory_resource>
#include <somnium/input/input.hpp>
namespace somnium::debug_ui::detail {
class Inspector final {
  public:
    void draw(const scene::World& world, const input::Configuration& mapping,
              const input::State& inputs, bool replay, bool editing);
#if defined(SOMNIUM_ENABLE_ITERATION)
    void configure(const scene::CodecRegistry& codecs) {
        m_editor.configure(codecs);
    }
    void revision(std::uint64_t value) {
        m_revision = value;
    }
    void selectEntity(std::string_view id) {
        m_selected = id;
        m_scrollToSelection = true;
        m_editor.select({});
    }
    void selectComponent(std::string_view component) {
        m_editor.select(component);
    }
    [[nodiscard]] std::vector<std::string> takeCommands() {
        return m_editor.takeCommands();
    }
#endif

  private:
    struct Ancestor {
        entt::entity entity;
        bool visibleChildren;
    };
    void hierarchy(const scene::World& world);
    void transform(const scene::World& world);
    std::pmr::vector<Ancestor> m_ancestors;
    std::string m_selected;
    bool m_scrollToSelection = false;
#if defined(SOMNIUM_ENABLE_ITERATION)
    SceneEditor m_editor;
    std::uint64_t m_revision = 0;
#endif
};
} // namespace somnium::debug_ui::detail
