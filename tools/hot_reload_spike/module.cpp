// Compiles independent generations against the unchanged Gameplay contract.
// A stable codec name carries state between distinct C++ layouts and storage IDs.
#include "api.hpp"
#include <array>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>

namespace somnium::spike::SPIKE_GENERATION {
struct State {
#ifdef SPIKE_NEW_LAYOUT
    std::array<std::uint64_t, 8> padding{};
#endif
    std::string label = "persistent game state";
    float distance = 0;
    std::uint32_t ticks = 0;
};
Result<void> registerCodecs(scene::CodecRegistry& codecs) {
    return codecs.add(
        {"game.SpikeState", 1,
         [](const scene::World& world, entt::entity entity) {
             return world.registry().all_of<State>(entity);
         },
         [](const scene::World& world, entt::entity entity) -> Result<scene::Json> {
             const auto& state = world.registry().get<State>(entity);
             return scene::Json{
                 {"distance", state.distance}, {"ticks", state.ticks}, {"label", state.label}};
         },
         [](scene::World& world, entt::entity entity, const scene::Json& value,
            scene::DeserializeContext&) -> Result<void> {
             auto fields = scene::requireFields(value, {"distance", "ticks", "label"});
             if (!fields)
                 return fields;
             auto distance = scene::readNumber(value, "distance");
             auto ticks = scene::readNumber(value, "ticks");
             auto label = scene::readString(value, "label");
             if (!distance)
                 return tl::unexpected(distance.error());
             if (!ticks)
                 return tl::unexpected(ticks.error());
             if (!label)
                 return tl::unexpected(label.error());
             if (*ticks < 0 || *ticks > 10000 ||
                 *ticks != static_cast<float>(static_cast<unsigned>(*ticks)))
                 return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid spike tick"});
             auto& state = world.registry().emplace_or_replace<State>(entity);
             state.distance = *distance;
             state.ticks = static_cast<std::uint32_t>(*ticks);
             state.label = std::move(*label);
             return {};
         }});
}
class Demo final : public runtime::Gameplay {
  public:
    Result<void> start(scene::World& world, const runtime::StartContext& context) override {
        const auto& configuration = context.mapping;
        auto player = world.find("player");
        auto move = configuration.axis("Move");
        if (!player)
            return tl::unexpected(player.error());
        if (!move)
            return tl::unexpected(move.error());
        m_player = *world.id(*player);
        m_move = *move;
        if (!world.registry().all_of<State>(*player))
            world.registry().emplace<State>(*player);
#ifdef SPIKE_FAIL_START
        auto local = world.registry().get<Transform>(*player).current;
        local.position.x += 999;
        auto changed = world.setLocal(*player, local);
        if (!changed)
            return changed;
        return tl::unexpected(
            Error{ErrorCode::InvalidState, "Intentional candidate start failure"});
#else
        m_running = true;
        return {};
#endif
    }
    Result<void> fixedUpdate(scene::World& world, const runtime::FixedUpdate& update) override {
        SOMNIUM_PROFILE_SCOPE("spike.game.step");
        auto player = world.find(m_player);
        if (!player)
            return tl::unexpected(player.error());
        auto& state = world.registry().get<State>(*player);
        if (!m_running || update.tick != state.ticks || update.inputs.step != update.tick)
            return tl::unexpected(Error{ErrorCode::InvalidState, "Tick was reset or replayed"});
#ifdef SPIKE_NEW_LAYOUT
        constexpr float speed = 2;
#else
        constexpr float speed = 1;
#endif
        const auto distance =
            update.inputs.axis(m_move) * speed * static_cast<float>(update.seconds);
        state.distance += distance;
        ++state.ticks;
        auto local = world.registry().get<Transform>(*player).current;
        local.position.x += distance;
        return world.setLocal(*player, local);
    }
    void stop(scene::World&) override {
        m_running = false;
    }

  private:
    std::string m_player;
    input::AxisId m_move;
    bool m_running = false;
};
runtime::Gameplay* create() {
    return new Demo;
}
void destroy(runtime::Gameplay* game) {
    delete game;
}
bool logIsShared() {
    return !Log::enabled("spike", Log::Level::Info) && Log::enabled("spike", Log::Level::Error);
}
const void* profiler() {
#ifdef SOMNIUM_ENABLE_PROFILING
    return &tracy::GetProfiler();
#else
    return nullptr;
#endif
}
} // namespace somnium::spike::SPIKE_GENERATION

#ifdef _WIN32
#define SOMNIUM_SPIKE_EXPORT __declspec(dllexport)
#else
#define SOMNIUM_SPIKE_EXPORT __attribute__((visibility("default")))
#endif
extern "C" SOMNIUM_SPIKE_EXPORT bool somniumSpikeQuery(std::uint32_t abi, std::size_t bytes,
                                                       somnium::spike::ModuleApi* output) {
    using namespace somnium::spike;
    namespace generation = somnium::spike::SPIKE_GENERATION;
#ifdef SPIKE_BAD_ABI
    constexpr auto expectedAbi = kAbi + 1;
#else
    constexpr auto expectedAbi = kAbi;
#endif
    if (abi != expectedAbi || bytes != sizeof(ModuleApi) || !output)
        return false;
    *output = {sizeof(generation::State),
               entt::type_hash<somnium::Transform>::value(),
               entt::type_hash<generation::State>::value(),
               generation::create,
               generation::destroy,
               generation::registerCodecs,
               generation::logIsShared,
               generation::profiler};
    return true;
}
