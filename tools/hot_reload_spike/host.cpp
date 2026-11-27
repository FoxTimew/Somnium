// Runs a CPU-only reload transaction over the real World, codecs and neutral replay.
// Images stay mapped until process exit because Tracy can retain source-location pointers.
#include "api.hpp"
#include <SDL3/SDL_loadso.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <somnium/core/conventions.hpp>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>
#include <somnium/platform/input_replay.hpp>

namespace {
using namespace somnium;
using spike::ModuleApi;
void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
template <typename T> T take(Result<T> value) {
    require(static_cast<bool>(value), value ? "" : value.error().message);
    return std::move(*value);
}
void checked(Result<void> value) {
    require(static_cast<bool>(value), value ? "" : value.error().message);
}
std::string read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    require(stream.good(), "Fixture cannot be read");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
class Images final {
  public:
    explicit Images(std::filesystem::path root) : m_root(std::move(root)) {
        std::error_code error;
        std::filesystem::create_directories(m_root, error);
        require(!error, error.message());
    }
    Result<SDL_SharedObject*> load(const std::filesystem::path& source) {
        if (m_images.size() >= 64)
            return tl::unexpected(
                Error{ErrorCode::InvalidState, "Restart after 64 retained images"});
        // Unique copies leave both the linker output and its debug symbols writable.
        const auto folder = m_root / std::to_string(m_sequence++);
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        if (error)
            return tl::unexpected(Error{ErrorCode::Io, error.message()});
        const auto copy = folder / source.filename();
        std::filesystem::copy_file(source, copy, std::filesystem::copy_options::overwrite_existing,
                                   error);
        if (error)
            return tl::unexpected(Error{ErrorCode::Io, error.message()});
        auto symbols = source;
        symbols.replace_extension(".pdb");
        const bool hasSymbols = std::filesystem::exists(symbols, error);
        if (error)
            return tl::unexpected(Error{ErrorCode::Io, error.message()});
        if (hasSymbols) {
            std::filesystem::copy_file(symbols, folder / symbols.filename(),
                                       std::filesystem::copy_options::overwrite_existing, error);
            if (error)
                return tl::unexpected(Error{ErrorCode::Io, error.message()});
        }
        auto* image = SDL_LoadObject(copy.string().c_str());
        if (!image)
            return tl::unexpected(Error{ErrorCode::Io, SDL_GetError()});
        m_images.push_back(image);
        return image;
    }
    static Result<ModuleApi> query(SDL_SharedObject* image) {
        auto symbol = SDL_LoadFunction(image, "somniumSpikeQuery");
        if (!symbol)
            return tl::unexpected(Error{ErrorCode::InvalidArgument, SDL_GetError()});
        auto query = reinterpret_cast<spike::Query>(symbol);
        ModuleApi api{};
        if (!query(spike::kAbi, sizeof(api), &api))
            return tl::unexpected(Error{ErrorCode::InvalidArgument, "Module ABI rejected"});
        if (api.transformId != entt::type_hash<Transform>::value() || !api.logIsShared())
            return tl::unexpected(
                Error{ErrorCode::InvalidState, "Duplicated engine identity or Log"});
#ifdef SOMNIUM_ENABLE_PROFILING
        if (api.profiler() != &tracy::GetProfiler())
            return tl::unexpected(Error{ErrorCode::InvalidState, "Duplicated Tracy client"});
#else
        require(api.profiler() == nullptr, "Unexpected profiling in plain configuration");
#endif
        return api;
    }
    [[nodiscard]] std::size_t count() const {
        return m_images.size();
    }

  private:
    std::filesystem::path m_root;
    std::vector<SDL_SharedObject*> m_images;
    std::size_t m_sequence = 0;
};
class Session final {
  public:
    struct StateInfo {
        std::size_t stateSize;
        entt::id_type stateId;
    };
    explicit Session(input::Configuration mapping)
        : m_input(std::move(mapping)), m_world(std::make_unique<scene::World>()) {
        checked(scene::registerBuiltinCodecs(m_codecs));
        const auto player = take(m_world->create("player"));
        m_world->registry().get<Name>(player).value = "preserved engine component";
        checked(m_world->propagate());
    }
    ~Session() {
        retire();
    }
    std::string snapshot() const {
        return take(scene::serialize(*m_world, m_codecs));
    }
    void start(SDL_SharedObject* image) {
        m_image = image;
        m_api = take(Images::query(image));
        checked(m_api.registerCodecs(m_codecs));
        m_game = m_api.create();
        require(m_game != nullptr, "Factory returned null");
        checked(m_game->start(*m_world, {m_input.configuration(), m_tick}));
    }
    bool reload(Images& images, const std::filesystem::path& source) {
        m_game->stop(*m_world);
        const auto saved = snapshot();
        auto* previousImage = m_image;
        const auto previousStateId = m_api.stateId;
        const auto previousSize = m_world->size();
        m_api.destroy(m_game);
        m_game = nullptr;
        // No owning groups, context entries or listeners exist in this experiment.
        // reset(), unlike clear(), removes the type-erased storage and its vtable.
        m_codecs = {};
        require(m_world->registry().reset(m_api.stateId), "Old game pool was not retired");
        require(m_world->registry().storage(m_api.stateId) == nullptr,
                "Old storage survived reset");
        m_api = {};
        m_image = nullptr;
        auto loaded = images.load(source);
        auto candidateApi =
            loaded ? Images::query(*loaded) : Result<ModuleApi>(tl::unexpected(loaded.error()));
        bool accepted = false;
        if (candidateApi) {
            scene::CodecRegistry codecs;
            checked(scene::registerBuiltinCodecs(codecs));
            checked(candidateApi->registerCodecs(codecs));
            auto restored = scene::deserialize(saved, codecs);
            if (restored) {
                auto* candidate = candidateApi->create();
                require(candidate != nullptr, "Candidate factory returned null");
                auto started = candidate->start(**restored, {m_input.configuration(), m_tick});
                if (started) {
                    m_world = std::move(*restored);
                    m_game = candidate;
                    m_codecs = std::move(codecs);
                    m_api = *candidateApi;
                    m_image = *loaded;
                    accepted = true;
                } else {
                    std::cout << "Expected rollback: " << started.error().message << '\n';
                    candidate->stop(**restored);
                    candidateApi->destroy(candidate);
                }
            }
        } else {
            std::cout << "Expected rejection: " << candidateApi.error().message << '\n';
        }
        if (!accepted) {
            checked(scene::registerBuiltinCodecs(m_codecs));
            const auto restoredApi = take(Images::query(previousImage));
            checked(restoredApi.registerCodecs(m_codecs));
            m_world = take(scene::deserialize(saved, m_codecs));
            m_api = restoredApi;
            m_image = previousImage;
            m_game = m_api.create();
            checked(m_game->start(*m_world, {m_input.configuration(), m_tick}));
            require(m_api.stateId == previousStateId, "Rollback selected the wrong generation");
        }
        require(m_world->size() == previousSize, "World identities were lost");
        require(snapshot() == saved, "Reload changed canonical serialized state");
        require(m_world->registry().get<Name>(take(m_world->find("player"))).value ==
                    "preserved engine component",
                "Engine component changed");
        return accepted;
    }
    void advance(platform::InputReplay& replay, std::uint64_t end) {
        for (; m_tick < end; ++m_tick) {
            checked(m_input.enqueue(take(replay.next(m_tick))));
            const auto inputs = take(m_input.advance(m_tick));
            m_world->beginStep();
            checked(m_game->fixedUpdate(*m_world, {core::kFixedStepSeconds, m_tick, inputs}));
            checked(m_world->propagate());
        }
    }
    float position() const {
        return m_world->registry().get<Transform>(take(m_world->find("player"))).current.position.x;
    }
    StateInfo stateInfo() const {
        return {m_api.stateSize, m_api.stateId};
    }

  private:
    void retire() {
        if (!m_game)
            return;
        m_game->stop(*m_world);
        m_api.destroy(m_game);
        m_game = nullptr;
        m_codecs = {};
        require(m_world->registry().reset(m_api.stateId), "Final game pool missing");
        m_api = {};
        m_image = nullptr;
    }
    input::Input m_input;
    std::unique_ptr<scene::World> m_world;
    scene::CodecRegistry m_codecs;
    SDL_SharedObject* m_image = nullptr;
    ModuleApi m_api{};
    runtime::Gameplay* m_game = nullptr;
    std::uint64_t m_tick = 0;
};
} // namespace

int main(int argc, char** argv) {
    using namespace somnium;
    require(argc == 7, "Expected v1/v2/bad-start/bad-ABI/replay/shadow arguments");
    Log::setCategoryLevel("spike", Log::Level::Error);
    const auto mapping = take(input::Configuration::parse(R"({"version":1,"actions":[],"axes":[
        {"name":"Move","mode":"rate","bindings":[{"source":"key","control":"W"}]}]})"));
    auto replay = take(platform::InputReplay::parse(read(argv[5])));
    Images images(argv[6]);
    Session session(mapping);
    session.start(take(images.load(argv[1])));
    const auto first = session.stateInfo();
    session.advance(replay, 30);
    require(std::abs(session.position() - .5F) < 1.e-5F, "v1 speed is wrong");
    require(session.reload(images, argv[2]), "v2 was rejected");
    const auto second = session.stateInfo();
    require(second.stateSize > first.stateSize && second.stateId != first.stateId,
            "Layout/type-generation change was not exercised");
    session.advance(replay, 60);
    require(std::abs(session.position() - 1.5F) < 1.e-5F, "v2 did not preserve state/change speed");
    require(!session.reload(images, argv[3]), "Failing start should roll back");
    require(!session.reload(images, argv[4]), "Invalid ABI should roll back");
    const auto invalid = std::filesystem::path(argv[6]) / "broken.module";
    {
        std::ofstream stream(invalid);
        stream << "not a library";
    }
    require(!session.reload(images, invalid), "Invalid binary should roll back");
    session.advance(replay, 65);
    require(std::abs(session.position() - 1.5F) < 1.e-5F,
            "Release transition was lost during reload");
    session.advance(replay, 70);
    require(std::abs(session.position() - (1.5F + 10.F / 60.F)) < 1.e-5F,
            "Rollback did not resume v2 at current tick");
    for (int cycle = 0; cycle < 10; ++cycle)
        require(session.reload(images, argv[2]), "Repeated reload failed");
    // Replacing a published input while its shadow is mapped must remain possible.
    const auto published =
        std::filesystem::path(argv[6]) / std::filesystem::path(argv[1]).filename();
    std::error_code error;
    std::filesystem::copy_file(argv[1], published,
                               std::filesystem::copy_options::overwrite_existing, error);
    require(!error, error.message());
    (void)take(images.load(published));
    std::filesystem::copy_file(argv[2], published,
                               std::filesystem::copy_options::overwrite_existing, error);
    require(!error, "Published module was locked despite shadow copy");
    std::cout << "PASS: World/IDs/codecs/state preserved; layouts " << first.stateSize << " -> "
              << second.stateSize
              << "; 70 replay ticks; v1/v2 speeds; 3 rollback paths; 10 cycles; "
              << "shared Log/Tracy; writable publication; retained images=" << images.count()
              << '\n';
}
