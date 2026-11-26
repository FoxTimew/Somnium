// Exercises the actual scene codecs and game through synthetic fixed-step states.
// No window, renderer, device, model loading or operating-system input is involved.
#include <doctest/doctest.h>
#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <somnium/game/codecs.hpp>
#include <somnium/game/sandbox_game.hpp>

namespace somnium::game {
namespace {
std::string read(const char* path) {
    std::ifstream stream(path);
    REQUIRE(stream.good());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
struct Fixture {
    scene::CodecRegistry codecs;
    std::unique_ptr<scene::World> world;
    input::Configuration mapping;
    SandboxGame game;
    std::vector<input::ButtonState> actions;
    std::vector<float> axes;
    std::uint64_t tick = 0;
    Fixture() {
        REQUIRE(scene::registerBuiltinCodecs(codecs));
        REQUIRE(registerCodecs(codecs));
        auto loaded = scene::deserialize(read(SOMNIUM_GAME_FIXTURE), codecs);
        REQUIRE(loaded);
        world = std::move(*loaded);
        auto configured = input::Configuration::parse(read(SOMNIUM_GAME_MAPPING));
        REQUIRE(configured);
        mapping = std::move(*configured);
        actions.resize(mapping.actions().size());
        axes.resize(mapping.axes().size());
        REQUIRE(game.start(*world, mapping));
    }
    ~Fixture() {
        game.stop(*world);
    }
    entt::entity entity(std::string_view id) {
        auto found = world->find(id);
        REQUIRE(found);
        return *found;
    }
    template <typename T> T& get(std::string_view id) {
        return world->registry().get<T>(entity(id));
    }
    void axis(std::string_view name, float value) {
        auto id = mapping.axis(name);
        REQUIRE(id);
        axes[id->value] = value;
    }
    void press(std::string_view name) {
        auto id = mapping.action(name);
        REQUIRE(id);
        actions[id->value].pressed = true;
    }
    void step(int count = 1) {
        for (int i = 0; i < count; ++i) {
            const input::State state{tick, actions, axes, {}};
            world->beginStep();
            auto result = game.fixedUpdate(*world, {1. / 60., tick, state});
            INFO((result ? "ok" : result.error().message));
            REQUIRE(result);
            REQUIRE(world->propagate());
            ++tick;
            std::fill(actions.begin(), actions.end(), input::ButtonState{});
        }
    }
    void place(glm::vec3 position) {
        auto pose = get<Transform>("character").current;
        pose.position = position;
        REQUIRE(world->teleport(entity("character"), pose));
    }
    glm::vec3 position() {
        return get<Transform>("character").current.position;
    }
};
} // namespace

TEST_CASE("Character travels at authored speed with normalized diagonal input") {
    for (float yaw : {0.F, .25F, glm::radians(90.F), glm::radians(-90.F), glm::radians(180.F)}) {
        Fixture f;
        auto& camera = f.get<FollowCamera>("camera");
        camera.orbit.yaw = yaw;
        f.axis("MoveX", 1);
        f.step(60);
        const auto right = f.get<Transform>("camera").current.rotation * glm::vec3(1, 0, 0);
        CHECK(glm::length(f.position() - glm::vec3(0, 0, 4) - 3.F * right) < .00002F);
        CHECK(f.position().y == 0);
        f.place({0, 0, 4});
        f.axis("MoveX", 0);
        f.axis("MoveZ", -1);
        camera.orbit.pitch = glm::radians(75.F);
        camera.orbit.distance = 30;
        f.step(60);
        auto forward = f.get<Transform>("camera").current.rotation * glm::vec3(0, 0, -1);
        forward.y = 0;
        forward = glm::normalize(forward);
        CHECK(glm::length(f.position() - glm::vec3(0, 0, 4) - 3.F * forward) < .00002F);
        CHECK(f.position().y == 0);
        const auto visual = f.get<Transform>("character-visual").current.rotation;
        CHECK(glm::length(visual * glm::vec3(0, 0, -1) - forward) < .00001F);
        f.place({0, 0, 4});
        f.axis("MoveX", 1);
        f.axis("MoveZ", 1);
        f.step(60);
        CHECK(glm::length(f.position() - glm::vec3(0, 0, 4)) == doctest::Approx(3).epsilon(.00001));
    }
}
TEST_CASE("Orbit and reset affect camera-relative movement in the same fixed step") {
    Fixture f;
    const auto initial = f.get<FollowCamera>("camera").orbit;
    f.get<FollowCamera>("camera").orbit.yaw = 0;
    f.axis("MouseYaw", glm::radians(180.F));
    f.axis("MoveZ", -1);
    f.step();
    CHECK(f.position().x == doctest::Approx(-.05F));
    CHECK(f.position().z == doctest::Approx(4.F));
    const auto previous = f.position();
    f.press("Reset");
    f.step();
    CHECK(f.position().x - previous.x == doctest::Approx(-.05F * std::sin(initial.yaw)));
    CHECK(f.position().z - previous.z == doctest::Approx(-.05F * std::cos(initial.yaw)));
}
TEST_CASE("Swept separated axes stop at walls while retaining tangential motion") {
    Fixture f;
    f.get<FollowCamera>("camera").orbit.yaw = glm::radians(90.F);
    f.place({2, 0, 0});
    f.axis("MoveX", 1);
    f.axis("MoveZ", 1);
    f.step(60);
    CHECK(f.position().x == doctest::Approx(2 + 3 / std::sqrt(2.F)).epsilon(.00001));
    CHECK(f.position().z == doctest::Approx(-1.45F));
    CHECK(f.position().y == 0);
}
TEST_CASE("A single high-speed step cannot cross a closed door or a thin wall") {
    Fixture f;
    f.get<FollowCamera>("camera").orbit.yaw = glm::radians(90.F);
    f.get<Character>("character").speed = 6000;
    f.axis("MoveX", 1);
    f.step();
    CHECK(f.position().z == doctest::Approx(-1.5F));
    f.place({2, 0, 4});
    f.step();
    CHECK(f.position().z == doctest::Approx(-1.45F));
}
TEST_CASE("A teleported overlapping character leaves by the shortest horizontal exit") {
    Fixture f;
    f.place({2, 0, -1.9F});
    f.step();
    CHECK(f.position().x == 2);
    CHECK(f.position().z == doctest::Approx(-1.45F));
}
TEST_CASE("Interact is proximity-limited and opens only the nearest door in sixty steps") {
    Fixture f;
    f.get<FollowCamera>("camera").orbit.yaw = glm::radians(90.F);
    f.press("Interact");
    f.step();
    CHECK(f.get<Door>("door-west").state == DoorState::Closed);
    f.place({0, 0, -.6F});
    f.press("Interact");
    f.axis("MoveX", 1);
    f.step(59);
    auto& door = f.get<Door>("door-west");
    CHECK(door.state == DoorState::Opening);
    CHECK(door.progress == doctest::Approx(59.F / 60.F));
    CHECK(f.position().z == doctest::Approx(-1.5F));
    f.step();
    CHECK(door.state == DoorState::Open);
    CHECK(door.progress == 1);
    const auto rotation = f.get<Transform>("door-west-pivot").current.rotation;
    CHECK(glm::length(rotation * glm::vec3(0, 0, 1) - glm::vec3(1, 0, 0)) < .00001F);
    CHECK(f.get<Door>("door-east").state == DoorState::Closed);
    f.step(50);
    CHECK(f.position().z < -3.9F);
}
TEST_CASE("Root follow camera tracks the character, clamps orbit and zoom, and resets") {
    Fixture f;
    const auto initial = f.get<FollowCamera>("camera").orbit;
    f.axis("MoveX", 1);
    f.step(10);
    auto& follow = f.get<FollowCamera>("camera");
    auto& transform = f.get<Transform>("camera");
    CHECK(f.get<Hierarchy>("camera").parent == entt::null);
    const auto target = f.position() + follow.offset;
    CHECK(glm::length(transform.current.position - target) == doctest::Approx(initial.distance));
    CHECK(glm::length(transform.current.rotation * glm::vec3(0, 0, -1) -
                      glm::normalize(target - transform.current.position)) < .00001F);
    CHECK(transform.current.position.x > transform.previous.position.x);
    f.axis("MoveX", 0);
    for (float sign : {-1.F, 1.F}) {
        f.axis("MousePitch", sign * 100);
        f.axis("MouseYaw", sign * 100);
        f.axis("WheelZoom", sign * 100);
        f.step();
        CHECK(follow.orbit.pitch >= glm::radians(5.F));
        CHECK(follow.orbit.pitch <= glm::radians(75.F));
        CHECK(follow.orbit.distance >= 4);
        CHECK(follow.orbit.distance <= 30);
        REQUIRE(scene::validateTrs(transform.current));
    }
    f.press("Reset");
    f.step();
    CHECK(follow.orbit.yaw == initial.yaw);
    CHECK(follow.orbit.pitch == initial.pitch);
    CHECK(follow.orbit.distance == initial.distance);
    CHECK(transform.current.position == transform.previous.position);
}
TEST_CASE("Game codecs round trip active doors and entity references through shared resolution") {
    Fixture f;
    f.place({0, 0, -.6F});
    f.press("Interact");
    f.step(30);
    auto saved = scene::serialize(*f.world, f.codecs);
    REQUIRE(saved);
    auto loaded = scene::deserialize(*saved, f.codecs);
    REQUIRE(loaded);
    auto& copy = **loaded;
    const auto character = *copy.find("character");
    CHECK(copy.registry().get<Character>(character).visual == *copy.find("character-visual"));
    const auto door = *copy.find("door-west");
    CHECK(copy.registry().get<Door>(door).pivot == *copy.find("door-west-pivot"));
    CHECK(copy.registry().get<Door>(door).progress == doctest::Approx(.5));
    CHECK(copy.registry().get<Door>(door).state == DoorState::Opening);
    CHECK(copy.registry().get<FollowCamera>(copy.activeCamera).target == character);
    SandboxGame resumed;
    REQUIRE(resumed.start(copy, f.mapping));
    for (std::uint64_t tick = 0; tick < 30; ++tick) {
        const input::State state{tick, f.actions, f.axes, {}};
        copy.beginStep();
        REQUIRE(resumed.fixedUpdate(copy, {1. / 60., tick, state}));
        REQUIRE(copy.propagate());
    }
    CHECK(copy.registry().get<Door>(door).state == DoorState::Open);
    const auto rotation =
        copy.registry().get<Transform>(*copy.find("door-west-pivot")).current.rotation;
    CHECK(glm::length(rotation * glm::vec3(0, 0, 1) - glm::vec3(1, 0, 0)) < .00001F);
    resumed.stop(copy);
}
TEST_CASE("Checked game codecs reject malformed data with component context") {
    Fixture f;
    const auto source = scene::Json::parse(read(SOMNIUM_GAME_FIXTURE));
    for (const auto& field : {"halfExtent", "duration", "pitchDegrees", "pivot"}) {
        auto data = source;
        for (auto& entity : data["entities"]) {
            for (auto& component : entity["components"]) {
                auto& value = component["data"];
                if (!value.contains(field))
                    continue;
                if (std::string_view(field) == "halfExtent")
                    value[field] = {-1, 1, 1};
                else if (std::string_view(field) == "pivot")
                    value[field] = "missing-pivot";
                else
                    value[field] = -1;
            }
        }
        auto rejected = scene::deserialize(data.dump(), f.codecs);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().message.find("game.") != std::string::npos);
    }
}
} // namespace somnium::game
