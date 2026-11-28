// Registers component codecs without teaching the engine about game component types.
// Entity references share one deferred resolver after all identities have been created.
#pragma once
#include <functional>
#include <nlohmann/json.hpp>
#include <somnium/core/export.hpp>
#include <somnium/scene/world.hpp>

namespace somnium::scene {
using Json = nlohmann::json;
class DeserializeContext final {
  public:
    explicit DeserializeContext(World& world) : m_world(world) {}
    using EntitySetter = std::function<Result<void>(World&, entt::entity, entt::entity)>;
    // Capture IDs/values, never component addresses: EnTT may move storage while loading.
    SOMNIUM_ENGINE_API void deferEntity(entt::entity owner, std::string field, std::string id,
                                        EntitySetter setter);
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> resolveEntities();
    [[nodiscard]] Result<assets::AssetHandle<assets::Model>> requestModel(std::string_view path) {
        return m_world.requestModel(path);
    }

  private:
    struct Reference {
        entt::entity owner;
        std::string field, id;
        EntitySetter setter;
    };
    World& m_world;
    std::vector<Reference> m_references;
};
#if defined(SOMNIUM_ENABLE_ITERATION)
enum class FieldPresentation : std::uint8_t { QuaternionEuler, RadiansDegrees };
struct EditorDescription {
    Json initial = Json::object();
    std::map<std::string, FieldPresentation, std::less<>> fields;
};
#endif
struct ComponentCodec {
    std::string name;
    std::uint32_t version = 1;
    std::function<bool(const World&, entt::entity)> contains;
    std::function<Result<Json>(const World&, entt::entity)> serialize;
    std::function<Result<void>(World&, entt::entity, const Json&, DeserializeContext&)> deserialize;
    // Explicit version migration; no guessed fields or silent unit conversions.
    std::function<Result<Json>(std::uint32_t, const Json&)> migrate = {};
    // Transient interpolation state uses the same registry, but never enters a scene file.
    std::function<Result<Json>(const World&, entt::entity)> captureTemporal = {};
    std::function<Result<void>(World&, entt::entity, const Json&)> restoreTemporal = {};
#if defined(SOMNIUM_ENABLE_ITERATION)
    // Data-only hints; codecs never depend on an editor or UI callback.
    EditorDescription editor = {};
#endif
};
class CodecRegistry final {
  public:
    [[nodiscard]] SOMNIUM_ENGINE_API Result<void> add(ComponentCodec codec);
    [[nodiscard]] SOMNIUM_ENGINE_API const ComponentCodec* find(std::string_view name) const;
    [[nodiscard]] const std::map<std::string, ComponentCodec, std::less<>>& codecs() const {
        return m_codecs;
    }

  private:
    std::map<std::string, ComponentCodec, std::less<>> m_codecs;
};
[[nodiscard]] SOMNIUM_ENGINE_API Result<void> registerBuiltinCodecs(CodecRegistry& codecs);
[[nodiscard]] SOMNIUM_ENGINE_API Result<std::string> serialize(const World& world,
                                                               const CodecRegistry& codecs);
struct WorldSnapshot {
    std::string canonical;
    std::string temporal;
    [[nodiscard]] std::size_t retainedBytes() const {
        return sizeof(WorldSnapshot) + canonical.capacity() + temporal.capacity();
    }
};
[[nodiscard]] SOMNIUM_ENGINE_API Result<WorldSnapshot> snapshot(const World& world,
                                                                const CodecRegistry& codecs);
[[nodiscard]] SOMNIUM_ENGINE_API Result<std::unique_ptr<World>>
restore(const WorldSnapshot& snapshot, const CodecRegistry& codecs, AssetContext* assets = nullptr);
// A successful result owns every model request; failure leaves the caller's active World intact.
[[nodiscard]] SOMNIUM_ENGINE_API Result<std::unique_ptr<World>>
deserialize(std::string_view text, const CodecRegistry& codecs, AssetContext* assets = nullptr);
[[nodiscard]] SOMNIUM_ENGINE_API Result<std::unique_ptr<World>>
load(const core::ByteReader& reader, std::string_view path, const CodecRegistry& codecs,
     AssetContext* assets = nullptr);

// Checked field readers are shared by engine and game codecs; no throwing JSON access.
[[nodiscard]] SOMNIUM_ENGINE_API Result<std::string> readString(const Json& object,
                                                                std::string_view field);
[[nodiscard]] SOMNIUM_ENGINE_API Result<float> readNumber(const Json& object,
                                                          std::string_view field);
[[nodiscard]] SOMNIUM_ENGINE_API Result<glm::vec3> readVector3(const Json& object,
                                                               std::string_view field);
[[nodiscard]] SOMNIUM_ENGINE_API Result<glm::quat> readQuaternion(const Json& object,
                                                                  std::string_view field);
[[nodiscard]] SOMNIUM_ENGINE_API Result<void>
requireFields(const Json& object, std::initializer_list<std::string_view> fields);
} // namespace somnium::scene
