// Exercises real identities, hierarchy and scene transactions entirely on the CPU.
// Imported model fixtures also verify shared-asset lifetime across staged Worlds.
#include <doctest/doctest.h>
#include <fstream>
#include <iterator>
#include <limits>
#include <somnium/scene/serialization.hpp>

namespace {
using namespace somnium;
using namespace somnium::scene;
entt::entity entity(World& world, std::string_view id) {
    auto created = world.create(id);
    REQUIRE(created);
    return *created;
}
CodecRegistry codecs() {
    CodecRegistry result;
    REQUIRE(registerBuiltinCodecs(result));
    return result;
}
void matrixNear(const glm::mat4& actual, const glm::mat4& expected) {
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            CHECK(actual[column][row] == doctest::Approx(expected[column][row]).epsilon(.001));
}
Json record(std::string id, Json extra = Json::object()) {
    extra["somnium.Transform"] = {
        {"version", 1},
        {"data", {{"position", {0, 0, 0}}, {"rotationDegrees", {0, 0, 0}}, {"scale", {1, 1, 1}}}}};
    return {{"id", std::move(id)}, {"components", std::move(extra)}};
}
Json document() {
    return {{"version", 1},
            {"nextId", 1},
            {"activeCamera", nullptr},
            {"settings", {{"ambient", .15}, {"clearColor", {0, 0, 0, 1}}}},
            {"entities", Json::array()}};
}
Json mesh(std::string path) {
    return {{"somnium.MeshRenderer", {{"version", 1}, {"data", {{"model", std::move(path)}}}}}};
}
struct FixtureReader final : core::ByteReader {
    Result<core::Bytes> read(std::string_view path) const override {
        std::ifstream file(std::string(SOMNIUM_ASSET_FIXTURES) + "/" + std::string(path),
                           std::ios::binary);
        if (!file)
            return tl::unexpected(
                Error{ErrorCode::NotFound, "Missing fixture " + std::string(path)});
        const std::vector<char> data{std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>()};
        const auto bytes = std::as_bytes(std::span(data));
        return core::Bytes(bytes.begin(), bytes.end());
    }
};
} // namespace

TEST_CASE("Persistent IDs outlive EnTT recycling and Name is not an identity") {
    World world;
    auto first = world.create();
    REQUIRE(first);
    auto firstId = world.id(*first);
    REQUIRE(firstId);
    const auto saved = std::string(*firstId);
    world.registry().get<Name>(*first).value = "duplicate label";
    const auto other = entity(world, "authored");
    world.registry().get<Name>(other).value = "duplicate label";
    CHECK_FALSE(world.find("duplicate label"));
    REQUIRE(world.destroy(*first));
    CHECK_FALSE(world.find(saved));
    auto second = world.create();
    REQUIRE(second);
    CHECK(*world.id(*second) != saved);
    CHECK(*world.find(*world.id(*second)) == *second);
    CHECK_FALSE(world.create("authored"));
    REQUIRE(world.create("runtime:0000000000000040"));
    CHECK(world.nextId() == 65);
    REQUIRE(world.create());
    CHECK(world.find("runtime:0000000000000041"));
    REQUIRE(world.advanceNextId(std::numeric_limits<std::uint64_t>::max()));
    CHECK_FALSE(world.create());
    CHECK_FALSE(world.advanceNextId(1));
}
TEST_CASE("Hierarchy mutations are immediate, reject cycles and maintain sibling links") {
    World world;
    const auto root = entity(world, "root");
    const auto first = entity(world, "first");
    const auto second = entity(world, "second");
    const auto grandchild = entity(world, "grandchild");
    REQUIRE(world.reparent(first, root));
    REQUIRE(world.reparent(second, root));
    REQUIRE(world.reparent(grandchild, first));
    const auto& parent = world.registry().get<Hierarchy>(root);
    CHECK(parent.firstChild == first);
    CHECK(parent.lastChild == second);
    CHECK(world.registry().get<Hierarchy>(first).nextSibling == second);
    CHECK(world.registry().get<Hierarchy>(second).previousSibling == first);
    CHECK_FALSE(world.reparent(root, grandchild));
    CHECK_FALSE(world.reparent(first, first));
    CHECK(world.registry().get<Hierarchy>(root).parent == entt::null);
    {
        const auto traversal = world.traverse();
        CHECK(traversal.entities().size() == 4);
        CHECK(world.orderIndex(root) < world.orderIndex(first));
        CHECK(world.orderIndex(first) < world.orderIndex(grandchild));
    }
    REQUIRE(world.reparent(second, entt::null));
    CHECK(parent.lastChild == first);
    CHECK(world.registry().get<Hierarchy>(first).nextSibling == entt::null);
    REQUIRE(world.destroy(first));
    CHECK_FALSE(world.find("first"));
    CHECK_FALSE(world.find("grandchild"));
    CHECK(parent.firstChild == entt::null);
    CHECK(parent.lastChild == entt::null);
    CHECK(world.size() == 2);
}
TEST_CASE("Propagation composes local TRS and teleport synchronizes the subtree") {
    World world;
    const auto root = entity(world, "root");
    const auto child = entity(world, "child");
    REQUIRE(world.reparent(child, root));
    Trs parentLocal;
    parentLocal.position = {3, 0, 0};
    parentLocal.rotation = glm::angleAxis(glm::radians(90.F), glm::vec3(0, 1, 0));
    Trs childLocal;
    childLocal.position = {2, 0, 0};
    REQUIRE(world.teleport(root, parentLocal));
    REQUIRE(world.teleport(child, childLocal));
    REQUIRE(world.propagate());
    matrixNear(world.registry().get<Transform>(child).world,
               localMatrix(parentLocal) * localMatrix(childLocal));
    CHECK(world.registry().get<Transform>(child).world[3].z == doctest::Approx(-2));
    world.beginStep();
    parentLocal.position.x = 7;
    childLocal.position.y = 4;
    REQUIRE(world.setLocal(root, parentLocal));
    REQUIRE(world.setLocal(child, childLocal));
    CHECK(world.registry().get<Transform>(root).previous.position.x == 3);
    REQUIRE(world.teleport(root, parentLocal));
    CHECK(world.registry().get<Transform>(child).previous.position.y == 4);
    parentLocal.scale.x = -1;
    CHECK_FALSE(world.setLocal(root, parentLocal));
    REQUIRE(world.teleport(root, parentLocal));
    REQUIRE(world.reparent(child, entt::null));
    CHECK(world.registry().get<Transform>(child).previous.position == childLocal.position);
    childLocal.scale.y = 0;
    CHECK_FALSE(world.teleport(child, childLocal));
}
TEST_CASE("Camera and light scale constraints reject mutation and reparent before changing World") {
    for (bool light : {false, true}) {
        World world;
        const auto scaled = entity(world, "scaled");
        const auto unitRoot = entity(world, "unit-root");
        const auto subject = entity(world, "subject");
        Trs scale;
        scale.scale = {2, 2, 2};
        REQUIRE(world.teleport(scaled, scale));
        if (light)
            world.registry().emplace<DirectionalLight>(subject);
        else
            world.registry().emplace<Camera>(subject);
        auto rejected = world.reparent(subject, scaled);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().message.find("subject") != std::string::npos);
        CHECK(rejected.error().message.find("scaled") != std::string::npos);
        CHECK(world.registry().get<Hierarchy>(subject).parent == entt::null);
        REQUIRE(world.reparent(subject, unitRoot));
        CHECK_FALSE(world.setLocal(unitRoot, scale));
        CHECK(world.registry().get<Transform>(unitRoot).current.scale == glm::vec3(1));
        CHECK(world.validate());
    }
}
TEST_CASE("JSON round trip preserves IDs, counter and rotations including Euler singularities") {
    auto table = codecs();
    for (const auto degrees :
         {glm::vec3(20, 35, 70), glm::vec3(45, 90, 10), glm::vec3(45, -90, 10)}) {
        World world;
        auto created = world.create();
        REQUIRE(created);
        const auto child = entity(world, "child");
        const auto camera = entity(world, "camera");
        world.registry().emplace<Camera>(camera);
        world.activeCamera = camera;
        const auto radians = glm::radians(degrees);
        Trs local;
        local.position = {1, 2, 3};
        local.rotation = glm::angleAxis(radians.z, glm::vec3(0, 0, 1)) *
                         glm::angleAxis(radians.y, glm::vec3(0, 1, 0)) *
                         glm::angleAxis(radians.x, glm::vec3(1, 0, 0));
        REQUIRE(world.teleport(*created, local));
        REQUIRE(world.reparent(child, *created));
        REQUIRE(world.propagate());
        auto output = serialize(world, table);
        REQUIRE(output);
        auto loaded = deserialize(*output, table);
        REQUIRE(loaded);
        auto copied = (*loaded)->find(*world.id(*created));
        REQUIRE(copied);
        CHECK((*loaded)->nextId() == world.nextId());
        matrixNear((*loaded)->registry().get<Transform>(*copied).world,
                   world.registry().get<Transform>(*created).world);
        CHECK((*loaded)->registry().get<Hierarchy>(*(*loaded)->find("child")).parent == *copied);
        CHECK(*(*loaded)->id((*loaded)->activeCamera) == "camera");
        CHECK(output->find("generation") == std::string::npos);
        CHECK(output->find("previous") == std::string::npos);
    }
}
TEST_CASE("Invalid scene data fails explicitly including unknown codecs and camera ancestors") {
    auto table = codecs();
    auto source = document();
    source["entities"] = Json::array({record("child"), record("parent")});
    CHECK(deserialize(source.dump(), table));
    for (std::string kind : {"unknown", "version", "duplicate", "missing-parent", "cycle", "scale",
                             "scale-light", "bad-camera", "bad-trs"}) {
        auto invalid = source;
        auto& components = invalid["entities"][0]["components"];
        if (kind == "unknown")
            components["game.Unregistered"] = {{"version", 1}, {"data", Json::object()}};
        if (kind == "version")
            invalid["version"] = 2;
        if (kind == "duplicate")
            invalid["entities"][1]["id"] = "child";
        if (kind == "missing-parent" || kind == "cycle" || kind == "scale" ||
            kind == "scale-light") {
            components["somnium.Hierarchy"] = {
                {"version", 1},
                {"data", {{"parent", kind == "missing-parent" ? "absent" : "parent"}}}};
            if (kind == "cycle")
                invalid["entities"][1]["components"]["somnium.Hierarchy"] = {
                    {"version", 1}, {"data", {{"parent", "child"}}}};
            if (kind == "scale" || kind == "scale-light") {
                if (kind == "scale")
                    components["somnium.Camera"] = {
                        {"version", 1},
                        {"data", {{"verticalFovDegrees", 60}, {"near", .1}, {"far", 100}}}};
                else
                    components["somnium.DirectionalLight"] = {{"version", 1},
                                                              {"data", {{"direction", {0, 0, 1}}}}};
                invalid["entities"][1]["components"]["somnium.Transform"]["data"]["scale"] = {2, 1,
                                                                                              1};
            }
        }
        if (kind == "bad-camera")
            invalid["activeCamera"] = "parent";
        if (kind == "bad-trs")
            components["somnium.Transform"]["data"]["position"] = "not a vector";
        auto loaded = deserialize(invalid.dump(), table);
        INFO(kind);
        REQUIRE_FALSE(loaded);
        CHECK_FALSE(loaded.error().message.empty());
    }
    CHECK_FALSE(deserialize("{", table));
}
TEST_CASE("Scene model requests share ownership and failed loads roll back only their requests") {
    FixtureReader reader;
    assets::ModelLoader loader;
    assets::AssetManager<assets::Model> manager(reader, loader);
    AssetContext context{manager};
    auto table = codecs();
    auto source = document();
    source["entities"] = Json::array({record("mesh", mesh("cube.glb"))});
    auto active = deserialize(source.dump(), table, &context);
    REQUIRE(active);
    const auto handle = (*active)->registry().get<MeshRenderer>(*(*active)->find("mesh")).model;
    CHECK(manager.get(handle));
    CHECK_FALSE(manager.unload(handle));
    {
        auto other = deserialize(source.dump(), table, &context);
        REQUIRE(other);
        CHECK((*other)->registry().get<MeshRenderer>(*(*other)->find("mesh")).model == handle);
    }
    CHECK(manager.get(handle));
    auto fresh = manager.identify("model.glb");
    REQUIRE(fresh);
    source["entities"].push_back(record("new-model", mesh("model.glb")));
    source["entities"].push_back(record("bad-model", mesh("absent.glb")));
    auto failed = deserialize(source.dump(), table, &context);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().message.find("bad-model") != std::string::npos);
    CHECK_FALSE(manager.state(*fresh));
    CHECK(manager.get(handle));
    auto output = serialize(**active, table);
    REQUIRE(output);
    CHECK(output->find("cube.glb") != std::string::npos);
    active->reset();
    CHECK_FALSE(manager.state(handle));
}
