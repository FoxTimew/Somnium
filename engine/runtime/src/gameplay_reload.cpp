// Rebuilds Worlds from the same codecs used for saving and Play/Stop.
// The retired World is inert after pool removal and holds asset leases until publication.
#include <set>
#include <somnium/core/logical_path.hpp>
#include <somnium/core/sha256.hpp>
#include <somnium/platform/file_watch.hpp>
#include <somnium/platform/module_loader.hpp>
#include <somnium/runtime/module_abi.hpp>
#include <somnium/runtime/reloadable_game.hpp>

namespace somnium::runtime {
namespace {
auto invalid(std::string message) {
    return tl::unexpected(Error{ErrorCode::InvalidArgument, std::move(message)});
}
struct Instance {
    GameModuleApi api;
    Gameplay* game = nullptr;
    ModuleRegistration registration;
    ~Instance() {
        if (game)
            api.destroy(game);
    }
};
Result<std::unique_ptr<Instance>> construct(GameModuleApi api) {
    auto instance = std::make_unique<Instance>();
    instance->api = api;
    auto builtins = scene::registerBuiltinCodecs(instance->registration.codecs);
    if (!builtins)
        return tl::unexpected(builtins.error());
    const auto builtinCount = instance->registration.codecs.codecs().size();
    auto game = api.create(instance->registration);
    if (!game)
        return tl::unexpected(game.error());
    instance->game = *game;
    if (!instance->game)
        return invalid("Module factory returned no gameplay");
    std::set<std::string> names;
    std::set<entt::id_type> ids{
        entt::type_hash<PersistentId>::value(),    entt::type_hash<Name>::value(),
        entt::type_hash<Transform>::value(),       entt::type_hash<Hierarchy>::value(),
        entt::type_hash<MeshRenderer>::value(),    entt::type_hash<Camera>::value(),
        entt::type_hash<DirectionalLight>::value()};
    scene::CodecRegistry builtinCodecs;
    (void)scene::registerBuiltinCodecs(builtinCodecs);
    for (const auto& storage : instance->registration.storages) {
        if (storage.codec.empty() || builtinCodecs.find(storage.codec) ||
            !instance->registration.codecs.find(storage.codec) || storage.size == 0 ||
            storage.alignment == 0 || !names.insert(storage.codec).second ||
            !ids.insert(storage.id).second)
            return invalid("Invalid or duplicate game storage/codec declaration");
    }
    if (instance->registration.codecs.codecs().size() != builtinCount + names.size())
        return invalid("Every game codec must declare its owned storage");
    return instance;
}
Result<core::Bytes> verifiedFile(platform::ModuleLoader& loader, const scene::Json& value) {
    auto fields = scene::requireFields(value, {"path", "sha256"});
    if (!fields)
        return tl::unexpected(fields.error());
    auto path = scene::readString(value, "path"), digest = scene::readString(value, "sha256");
    if (!path || !digest)
        return invalid("Module file requires path and sha256");
    auto logical = core::logicalPath(*path);
    if (!logical || *logical != *path || digest->size() != 64)
        return invalid("Invalid module path or digest");
    auto bytes = loader.read(*path);
    if (!bytes)
        return tl::unexpected(bytes.error());
    if (core::sha256(*bytes) != *digest)
        return invalid("Module file checksum mismatch: " + *path);
    return bytes;
}
Result<GameModuleApi> load(platform::ModuleLoader& loader, std::string_view publication,
                           const core::Bytes* observed = nullptr) {
    auto bytes = observed ? Result<core::Bytes>(*observed) : loader.read(publication);
    if (!bytes)
        return tl::unexpected(bytes.error());
    const auto document = scene::Json::parse(
        std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), nullptr,
        false);
    if (document.is_discarded() || !document.is_object() || !document.contains("version") ||
        document["version"] != 1 || !document.contains("binary"))
        return invalid("Invalid game module publication");
    auto fields = document.contains("symbols")
                      ? scene::requireFields(document, {"version", "binary", "symbols"})
                      : scene::requireFields(document, {"version", "binary"});
    if (!fields)
        return tl::unexpected(fields.error());
    auto binary = verifiedFile(loader, document["binary"]);
    if (!binary)
        return tl::unexpected(binary.error());
    const auto binaryPath = *scene::readString(document["binary"], "path");
    const auto binaryName = binaryPath.substr(binaryPath.find_last_of('/') + 1);
    core::Bytes symbols;
    std::string symbolsName;
    if (document.contains("symbols")) {
        auto data = verifiedFile(loader, document["symbols"]);
        if (!data)
            return tl::unexpected(data.error());
        symbols = std::move(*data);
        const auto path = *scene::readString(document["symbols"], "path");
        symbolsName = path.substr(path.find_last_of('/') + 1);
    }
    auto image = loader.load({binaryName, *binary, symbolsName, symbols});
    if (!image)
        return tl::unexpected(image.error());
    auto function = loader.function(*image, "somniumGameQuery");
    if (!function)
        return tl::unexpected(function.error());
    const auto query = reinterpret_cast<GameModuleQuery>(*function);
    GameModuleApi api;
    const auto fingerprint = engineModuleAbi();
    if (!query(kGameModuleVersion, sizeof(GameModuleApi), fingerprint.c_str(), &api) ||
        !api.create || !api.destroy)
        return tl::unexpected(
            Error{ErrorCode::RestartRequired, "Game module ABI incompatible: redémarrage requis"});
    return api;
}
void detach(Instance& instance, scene::World& world) {
    if (instance.registration.detach)
        instance.registration.detach(world);
}
void retire(Instance& instance, scene::World& world) {
    detach(instance, world);
    for (const auto& storage : instance.registration.storages)
        world.registry().reset(storage.id);
    // Owning groups may retain pool addresses. Never traverse this World again.
}
} // namespace
struct ReloadableGame::Impl {
    std::unique_ptr<platform::ModuleLoader> loader;
    std::unique_ptr<platform::FileWatch> watch;
    std::unique_ptr<Instance> active;
    scene::CodecRegistry codecs;
    std::string publication;
};
ReloadableGame::ReloadableGame(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
ReloadableGame::~ReloadableGame() = default;
Result<std::unique_ptr<ReloadableGame>> ReloadableGame::create(GameModuleConfig config) {
    auto loader = platform::ModuleLoader::create(config.root);
    if (!loader)
        return tl::unexpected(loader.error());
    auto watch = std::make_unique<platform::FileWatch>(**loader, std::string(config.publication));
    auto baseline = watch->poll(0);
    if (!baseline)
        return tl::unexpected(baseline.error());
    auto observed = std::move(*baseline);
    if (!observed)
        return invalid("Initial module publication is missing");
    // The loaded bytes must match the watch baseline, even if a build publishes concurrently.
    auto api = load(**loader, config.publication, &*observed);
    if (!api)
        return tl::unexpected(api.error());
    auto instance = construct(*api);
    if (!instance)
        return tl::unexpected(instance.error());
    auto impl = std::make_unique<Impl>();
    impl->loader = std::move(*loader);
    impl->watch = std::move(watch);
    impl->publication = config.publication;
    impl->active = std::move(*instance);
    impl->codecs = std::move(impl->active->registration.codecs);
    return std::unique_ptr<ReloadableGame>(new ReloadableGame(std::move(impl)));
}
const scene::CodecRegistry& ReloadableGame::codecs() const {
    return m_impl->codecs;
}
Result<void> ReloadableGame::start(scene::World& world, const StartContext& context) {
    return m_impl->active->game->start(world, context);
}
Result<void> ReloadableGame::fixedUpdate(scene::World& world, const FixedUpdate& update) {
    return m_impl->active->game->fixedUpdate(world, update);
}
void ReloadableGame::stop(scene::World& world) {
    m_impl->active->game->stop(world);
    detach(*m_impl->active, world);
}
Result<bool> ReloadableGame::changed(double seconds) {
    auto observed = m_impl->watch->poll(seconds);
    if (!observed)
        return tl::unexpected(observed.error());
    return observed->has_value();
}
bool ReloadableGame::usable() const {
    return m_impl->active && m_impl->active->game;
}
std::size_t ReloadableGame::imageCount() const {
    return m_impl->loader->imageCount();
}
std::size_t ReloadableGame::retainedBytes() const {
    return m_impl->loader->retainedBytes();
}
std::span<const ModuleStorage> ReloadableGame::storages() const {
    return m_impl->active->registration.storages;
}
Result<void> ReloadableGame::replace(std::unique_ptr<scene::World>& world,
                                     const StartContext& context, bool playing,
                                     scene::AssetContext* assets,
                                     std::span<const scene::WorldSnapshot* const> preserved,
                                     std::string_view publication) {
    if (!usable() || !world)
        return invalid("No active game/World to replace");
    if (playing)
        stop(*world);
    auto saved = scene::snapshot(*world, m_impl->codecs);
    if (!saved) {
        if (playing) {
            auto resumed = start(*world, context);
            if (!resumed) {
                stop(*world);
                m_impl->active.reset();
                return resumed;
            }
        }
        return tl::unexpected(saved.error());
    }
    auto previous = std::move(m_impl->active);
    const auto oldApi = previous->api;
    previous->api.destroy(previous->game);
    previous->game = nullptr;
    m_impl->codecs = {};
    retire(*previous, *world);
    auto retired = std::move(world);
    auto restoreInstance = [&](GameModuleApi api) -> Result<std::unique_ptr<Instance>> {
        auto instance = construct(api);
        if (!instance)
            return tl::unexpected(instance.error());
        auto& value = **instance;
        for (const auto* snapshot : preserved) {
            auto checked = scene::restore(*snapshot, value.registration.codecs, assets);
            if (!checked)
                return tl::unexpected(checked.error());
        }
        auto candidate = scene::restore(*saved, value.registration.codecs, assets);
        if (!candidate)
            return tl::unexpected(candidate.error());
        auto started = value.game->start(**candidate, context);
        auto valid = started ? (*candidate)->propagate() : started;
        if (!valid || !playing) {
            value.game->stop(**candidate);
            detach(value, **candidate);
            if (!valid)
                return tl::unexpected(valid.error());
            candidate = scene::restore(*saved, value.registration.codecs, assets);
            if (!candidate)
                return tl::unexpected(candidate.error());
        }
        world = std::move(*candidate);
        return instance;
    };
    auto api = load(*m_impl->loader, publication.empty() ? m_impl->publication : publication);
    Result<std::unique_ptr<Instance>> candidate =
        api ? restoreInstance(*api) : tl::unexpected(api.error());
    std::optional<Error> rejected;
    if (!candidate) {
        rejected = candidate.error();
        candidate = restoreInstance(oldApi);
        if (!candidate) {
            // Unrecoverable environmental loss (for example an asset deleted in both versions).
            return tl::unexpected(Error{ErrorCode::InvalidState,
                                        "Game rollback failed: " + candidate.error().message +
                                            "; candidate: " + rejected->message});
        }
    }
    m_impl->active = std::move(*candidate);
    m_impl->codecs = std::move(m_impl->active->registration.codecs);
    retired.reset();
    previous.reset();
    if (rejected)
        return tl::unexpected(*rejected);
    return {};
}
} // namespace somnium::runtime
