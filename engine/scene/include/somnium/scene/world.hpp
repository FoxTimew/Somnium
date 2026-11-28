// Owns identity, hierarchy and asset requests while exposing the EnTT registry directly.
// Structural operations are immediate and cannot invalidate a guarded traversal.
#pragma once
#include <entt/entity/registry.hpp>
#include <somnium/core/export.hpp>
#include <somnium/scene/components.hpp>
#include <unordered_map>

namespace somnium::scene {
struct AssetContext {
    assets::AssetManager<assets::Model>& models;
};
class World final {
  public:
    class Traversal final {
      public:
        SOMNIUM_ENGINE_API ~Traversal();
        Traversal(const Traversal&) = delete;
        Traversal& operator=(const Traversal&) = delete;
        [[nodiscard]] auto begin() const {
            return m_entities.begin();
        }
        [[nodiscard]] auto end() const {
            return m_entities.end();
        }
        [[nodiscard]] std::span<const entt::entity> entities() const {
            return m_entities;
        }

      private:
        friend class World;
        SOMNIUM_ENGINE_API explicit Traversal(const World& world);
        const World& m_world;
        std::span<const entt::entity> m_entities;
    };
    explicit World(AssetContext* assets = nullptr) : m_assets(assets) {}
    SOMNIUM_ENGINE_API ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    [[nodiscard]] entt::registry& registry() {
        return m_registry;
    }
    [[nodiscard]] const entt::registry& registry() const {
        return m_registry;
    }
    [[nodiscard]] SOMNIUM_ENGINE_API Result<entt::entity> create(std::string_view id = {});
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> destroy(entt::entity entity);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> reparent(entt::entity entity,
                                                           entt::entity parent);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<entt::entity> find(std::string_view id) const;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<std::string_view> id(entt::entity entity) const;
    [[nodiscard]] std::size_t size() const {
        return m_ids.size();
    }
    [[nodiscard]] std::uint64_t nextId() const {
        return m_nextId;
    }
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> advanceNextId(std::uint64_t next);
    SOMNIUM_ENGINE_API void reserve(std::size_t count);
    [[nodiscard]] Traversal traverse() const {
        return Traversal(*this);
    }
    [[nodiscard]] SOMNIUM_ENGINE_API std::size_t orderIndex(entt::entity entity) const;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> setLocal(entt::entity entity, const Trs& local);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> teleport(entt::entity entity, const Trs& local);
    SOMNIUM_ENGINE_API void beginStep();
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> propagate();
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> validate() const;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<assets::AssetHandle<assets::Model>>
    requestModel(std::string_view path);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<std::string_view>
    modelPath(assets::AssetHandle<assets::Model> handle) const;
    SceneSettings settings;
    entt::entity activeCamera = entt::null;

  private:
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> mutableStructure() const;
    [[nodiscard]] SOMNIUM_ENGINE_API bool owns(entt::entity entity) const;
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void>
    unitScales(entt::entity edited = entt::null, const Trs* local = nullptr,
               const entt::entity* parent = nullptr) const;
    SOMNIUM_ENGINE_API void rebuildOrder() const;
    SOMNIUM_ENGINE_API void unlink(entt::entity entity);
    SOMNIUM_ENGINE_API void synchronize(entt::entity root);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> assignLocal(entt::entity entity, const Trs& local,
                                                              bool teleport);
    entt::registry m_registry;
    std::map<std::string, entt::entity, std::less<>> m_ids;
    std::uint64_t m_nextId = 1;
    AssetContext* m_assets;
    std::vector<assets::AssetHandle<assets::Model>> m_requests;
    mutable std::vector<entt::entity> m_order, m_stack;
    mutable std::unordered_map<entt::entity, std::size_t> m_orderIndices;
    mutable std::size_t m_traversals = 0;
    mutable bool m_orderDirty = true;
};
[[nodiscard]] SOMNIUM_ENGINE_API glm::mat4 localMatrix(const Trs& local);
[[nodiscard]] SOMNIUM_ENGINE_API Result<void> validateTrs(const Trs& local);
} // namespace somnium::scene
