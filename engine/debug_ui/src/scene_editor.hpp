// Builds generic component drafts from codec values and data-only field hints.
// Applying a draft queues JSON; only runtime may commit it at a fixed-step boundary.
#pragma once
#include <somnium/scene/serialization.hpp>
namespace somnium::debug_ui::detail {
class SceneEditor final {
  public:
    void configure(const scene::CodecRegistry& codecs) {
        m_codecs = &codecs;
    }
    void draw(const scene::World& world, std::string_view selected, std::uint64_t revision);
    void select(std::string_view component);
    [[nodiscard]] std::vector<std::string> takeCommands();

  private:
    void queue(const scene::Json& command);
    const scene::CodecRegistry* m_codecs = nullptr;
    std::string m_component = "somnium.Transform", m_loadedComponent, m_loadedEntity;
    std::string m_raw, m_error, m_parent;
    scene::Json m_draft;
    std::uint64_t m_revision = ~std::uint64_t{};
    bool m_rawMode = false, m_adding = false;
    std::vector<std::string> m_commands;
};
} // namespace somnium::debug_ui::detail
