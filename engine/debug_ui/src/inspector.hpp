// Displays a read-only hierarchy/input view and stages local transform edits.
// Persistent IDs keep selection stable across registry storage changes.
#pragma once
#include "transform_edits.hpp"
#include <memory_resource>
#include <somnium/input/input.hpp>
namespace somnium::debug_ui::detail {
class Inspector final {
  public:
    void draw(const scene::World& world, const input::Configuration& mapping,
              const input::State& inputs, bool replay);
    void apply(scene::World& world);

  private:
    struct Ancestor {
        entt::entity entity;
        bool visibleChildren;
    };
    void hierarchy(const scene::World& world);
    void transform(const scene::World& world);
    std::pmr::vector<Ancestor> m_ancestors;
    std::string m_selected, m_error;
    Trs m_local;
    glm::vec3 m_degrees{0};
    bool m_editing = false;
    TransformEdits m_edits;
};
} // namespace somnium::debug_ui::detail
