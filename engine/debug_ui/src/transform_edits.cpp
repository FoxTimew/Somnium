// Applies queued teleports at a fixed-step boundary and reports rejected edits.
// Requests are consumed once; invalid or deleted targets cannot replay on later steps.
#include "transform_edits.hpp"
#include <optional>
namespace somnium::debug_ui::detail {
void TransformEdits::queue(std::string_view id, const Trs& local) {
    for (auto& pending : m_pending)
        if (pending.id == id) {
            pending.local = local;
            return;
        }
    m_pending.push_back({std::string(id), local});
}
Result<void> TransformEdits::apply(scene::World& world) {
    std::optional<Error> failure;
    for (const auto& pending : m_pending) {
        auto entity = world.find(pending.id);
        auto result = entity ? world.teleport(*entity, pending.local)
                             : Result<void>(tl::unexpected(entity.error()));
        if (!result && !failure)
            failure = result.error();
    }
    m_pending.clear();
    if (failure)
        return tl::unexpected(std::move(*failure));
    return {};
}
} // namespace somnium::debug_ui::detail
