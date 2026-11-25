// Coalesces editor requests by persistent identity without mutating a displayed World.
// Runtime drains requests only after beginStep; World remains the validation authority.
#pragma once
#include <somnium/scene/world.hpp>
namespace somnium::debug_ui::detail {
class TransformEdits final {
  public:
    void queue(std::string_view id, const Trs& local);
    [[nodiscard]] Result<void> apply(scene::World& world);
    [[nodiscard]] bool empty() const {
        return m_pending.empty();
    }

  private:
    struct Edit {
        std::string id;
        Trs local;
    };
    std::vector<Edit> m_pending;
};
} // namespace somnium::debug_ui::detail
