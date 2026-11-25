// Defines a game-owned component and codec using only the public scene registration API.
// Its pivot reference must use the same second pass as the engine hierarchy codec.
#include <doctest/doctest.h>
#include <somnium/scene/serialization.hpp>

namespace somnium::game {
struct DoorReference {
    entt::entity pivot = entt::null;
};
namespace {
using namespace scene;
Result<void> registerDoor(CodecRegistry& table) {
    return table.add(
        {"game.DoorReference", 1,
         [](const World& world, entt::entity entity) {
             return world.registry().all_of<DoorReference>(entity);
         },
         [](const World& world, entt::entity entity) -> Result<Json> {
             auto id = world.id(world.registry().get<DoorReference>(entity).pivot);
             if (!id)
                 return tl::unexpected(id.error());
             return Json{{"pivot", std::string(*id)}};
         },
         [](World& world, entt::entity entity, const Json& data,
            DeserializeContext& context) -> Result<void> {
             auto fields = requireFields(data, {"pivot"});
             if (!fields)
                 return fields;
             auto id = readString(data, "pivot");
             if (!id)
                 return tl::unexpected(id.error());
             world.registry().emplace<DoorReference>(entity);
             context.deferEntity(
                 entity, "game.DoorReference/pivot", std::move(*id),
                 [](World& target, entt::entity owner, entt::entity pivot) -> Result<void> {
                     target.registry().get<DoorReference>(owner).pivot = pivot;
                     return {};
                 });
             return {};
         }});
}
} // namespace
TEST_CASE("Game codec resolves a forward entity reference and round trips without engine edits") {
    CodecRegistry table;
    REQUIRE(registerBuiltinCodecs(table));
    REQUIRE(registerDoor(table));
    World original;
    auto door = original.create("a-door");
    auto pivot = original.create("z-pivot");
    REQUIRE(door);
    REQUIRE(pivot);
    original.registry().emplace<DoorReference>(*door, *pivot);
    REQUIRE(original.reparent(*door, *pivot));
    auto text = serialize(original, table);
    REQUIRE(text);
    auto data = Json::parse(*text);
    CHECK(data["entities"][0]["id"] == "a-door");
    CHECK(data["entities"][0]["components"]["game.DoorReference"]["data"]["pivot"] == "z-pivot");
    auto loaded = deserialize(*text, table);
    REQUIRE(loaded);
    const auto copiedDoor = *(*loaded)->find("a-door");
    const auto copiedPivot = *(*loaded)->find("z-pivot");
    CHECK((*loaded)->registry().get<DoorReference>(copiedDoor).pivot == copiedPivot);
    CHECK((*loaded)->registry().get<Hierarchy>(copiedDoor).parent == copiedPivot);
    auto roundTrip = serialize(**loaded, table);
    REQUIRE(roundTrip);
    CHECK(Json::parse(*roundTrip) == data);
    data["entities"][0]["components"]["game.DoorReference"]["data"]["pivot"] = "missing";
    auto rejected = deserialize(data.dump(), table);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().message.find("a-door/game.DoorReference/pivot") != std::string::npos);
    CHECK(rejected.error().message.find("missing") != std::string::npos);
    CHECK_FALSE(registerDoor(table));
}
} // namespace somnium::game
