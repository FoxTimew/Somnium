// Exercises codec-driven edits, persistence and real gameplay without a window or GPU.
// A test-only component proves that the editor has no built-in list of game types.
#include "edit_session.hpp"
#include "scene_editor.hpp"
#include <chrono>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <somnium/game/codecs.hpp>
#include <somnium/game/sandbox_game.hpp>

namespace {
using namespace somnium;
using namespace somnium::scene;
struct Widget {
    float value = 1, previous = 1;
    std::string label = "fixture";
};
ComponentCodec widgetCodec() {
    ComponentCodec codec{"consumer.Widget", 1,
                         [](const World& world, entt::entity entity) {
                             return world.registry().all_of<Widget>(entity);
                         },
                         [](const World& world, entt::entity entity) -> Result<Json> {
                             const auto& widget = world.registry().get<Widget>(entity);
                             return Json{{"value", widget.value}, {"label", widget.label}};
                         },
                         [](World& world, entt::entity entity, const Json& data,
                            DeserializeContext&) -> Result<void> {
                             auto fields = requireFields(data, {"value", "label"});
                             if (!fields)
                                 return fields;
                             auto value = readNumber(data, "value");
                             auto label = readString(data, "label");
                             if (!value)
                                 return tl::unexpected(value.error());
                             if (!label)
                                 return tl::unexpected(label.error());
                             if (*value < 0)
                                 return tl::unexpected(Error{ErrorCode::InvalidArgument,
                                                             "Widget value must be nonnegative"});
                             world.registry().emplace<Widget>(entity, *value, *value, *label);
                             return {};
                         }};
    codec.captureTemporal = [](const World& world, entt::entity entity) -> Result<Json> {
        return Json{{"previous", world.registry().get<Widget>(entity).previous}};
    };
    codec.restoreTemporal = [](World& world, entt::entity entity,
                               const Json& data) -> Result<void> {
        auto previous = readNumber(data, "previous");
        if (!previous)
            return tl::unexpected(previous.error());
        world.registry().get<Widget>(entity).previous = *previous;
        return {};
    };
    codec.editor.initial = {{"value", 1}, {"label", "fixture"}};
    return codec;
}
std::string canonical(const World& world, const CodecRegistry& codecs) {
    auto result = serialize(world, codecs);
    REQUIRE(result);
    return *result;
}
struct EmptyReader final : core::ByteReader {
    Result<core::Bytes> read(std::string_view) const override {
        return core::Bytes{};
    }
};
struct ModelLoader final : assets::Loader<assets::Model> {
    Result<assets::Model> load(const core::ByteReader&, std::string_view path) const override {
        if (path != "models/valid.glb")
            return tl::unexpected(Error{ErrorCode::NotFound, "Missing editor model"});
        return assets::Model{};
    }
};
struct Editing {
    EmptyReader reader;
    ModelLoader loader;
    assets::AssetManager<assets::Model> models{reader, loader};
    AssetContext assets{models};
    CodecRegistry codecs;
    std::unique_ptr<World> world = std::make_unique<World>(&assets);
    Editing() {
        REQUIRE(registerBuiltinCodecs(codecs));
        REQUIRE(game::registerCodecs(codecs));
        REQUIRE(codecs.add(widgetCodec()));
        REQUIRE(world->create("root"));
        REQUIRE(world->create("child"));
        REQUIRE(world->create("other"));
        REQUIRE(world->reparent(*world->find("child"), *world->find("root")));
    }
    Result<void> apply(const Json& command) {
        auto changed = runtime::editWorld(*world, codecs, command, &assets);
        if (!changed)
            return tl::unexpected(changed.error());
        world = std::move(changed->world);
        return {};
    }
    template <typename T> T& get(std::string_view id) {
        return world->registry().get<T>(*world->find(id));
    }
};
Json set(std::string id, std::string component, Json data, std::string op = "set") {
    return {{"op", std::move(op)},
            {"id", std::move(id)},
            {"component", std::move(component)},
            {"data", std::move(data)}};
}
Json transform(float x, float scale = 1) {
    return {{"position", {x, 0, 0}}, {"rotation", {0, 0, 0, 1}}, {"scale", {scale, scale, scale}}};
}
} // namespace
TEST_CASE("Generic editor creates, assigns, reparents and deletes through ordinary codecs") {
    Editing f;
    REQUIRE(f.apply(
        {{"op", "create"}, {"id", "authored"}, {"name", "New entity"}, {"parent", "root"}}));
    REQUIRE(
        f.apply(set("authored", "somnium.MeshRenderer", {{"model", "models/valid.glb"}}, "add")));
    const auto model = f.get<MeshRenderer>("authored").model;
    REQUIRE(f.apply(set("authored", "consumer.Widget",
                        f.codecs.find("consumer.Widget")->editor.initial, "add")));
    REQUIRE(
        f.apply(set("authored", "consumer.Widget", {{"value", 7}, {"label", "Game component"}})));
    REQUIRE(f.apply(set("authored", "somnium.Name", {{"value", "Renamed"}})));
    REQUIRE(f.apply(set("authored", "somnium.Transform", transform(3))));
    CHECK(f.get<Widget>("authored").value == 7);
    CHECK(f.get<Name>("authored").value == "Renamed");
    CHECK(f.get<Transform>("authored").world[3].x == 3);
    REQUIRE(f.apply({{"op", "reparent"}, {"id", "authored"}, {"parent", "child"}}));
    CHECK(f.get<Hierarchy>("authored").parent == *f.world->find("child"));
    REQUIRE(f.apply({{"op", "remove"}, {"id", "authored"}, {"component", "consumer.Widget"}}));
    REQUIRE(f.apply({{"op", "delete"}, {"id", "root"}}));
    CHECK_FALSE(f.world->find("root"));
    CHECK_FALSE(f.world->find("child"));
    CHECK_FALSE(f.world->find("authored"));
    CHECK(f.world->find("other"));
    CHECK_FALSE(f.models.state(model));
}
TEST_CASE("Invalid edits preserve the active World, history and all model requests") {
    Editing f;
    REQUIRE(f.apply(set("child", "somnium.MeshRenderer", {{"model", "models/valid.glb"}}, "add")));
    REQUIRE(f.apply(set("other", "game.Character", {{"speed", 3}, {"visual", "child"}}, "add")));
    REQUIRE(f.apply(set("child", "consumer.Widget", {{"value", 2}, {"label", "ok"}}, "add")));
    const auto original = snapshot(*f.world, f.codecs);
    REQUIRE(original);
    const auto handle = f.get<MeshRenderer>("child").model;
    for (const auto& command :
         {Json{{"op", "reparent"}, {"id", "root"}, {"parent", "child"}},
          Json{{"op", "delete"}, {"id", "child"}},
          Json{{"op", "remove"}, {"id", "root"}, {"component", "somnium.Transform"}},
          set("child", "somnium.MeshRenderer", {{"model", "models/missing.glb"}}),
          set("child", "consumer.Widget", {{"value", -1}, {"label", "bad"}}),
          set("root", "somnium.Transform", transform(0, 0)),
          set("root", "unknown.Component", Json::object(), "add")}) {
        CHECK_FALSE(f.apply(command));
        const auto after = snapshot(*f.world, f.codecs);
        REQUIRE(after);
        CHECK(after->canonical == original->canonical);
        CHECK(after->temporal == original->temporal);
        CHECK(f.models.get(handle));
    }
    REQUIRE(f.apply(set("child", "somnium.Camera",
                        {{"verticalFovRadians", 1}, {"near", .1}, {"far", 100}}, "add")));
    CHECK_FALSE(f.apply(set("root", "somnium.Transform", transform(0, 2))));
}
TEST_CASE("Unchanged codec histories survive while edited transform branches teleport") {
    Editing f;
    REQUIRE(f.apply(set("other", "consumer.Widget", {{"value", 9}, {"label", "keep"}}, "add")));
    f.get<Widget>("other").previous = 2;
    f.world->beginStep();
    REQUIRE(f.world->setLocal(*f.world->find("root"), {.position = {5, 0, 0}}));
    REQUIRE(f.world->setLocal(*f.world->find("child"), {.position = {3, 0, 0}}));
    REQUIRE(f.world->setLocal(*f.world->find("other"), {.position = {7, 0, 0}}));
    REQUIRE(f.apply(set("child", "somnium.Name", {{"value", "History unchanged"}})));
    CHECK(f.get<Transform>("root").previous.position.x == 0);
    CHECK(f.get<Transform>("child").previous.position.x == 0);
    CHECK(f.get<Widget>("other").previous == 2);
    REQUIRE(f.apply(set("root", "somnium.Transform", transform(8))));
    CHECK(f.get<Transform>("root").previous.position.x == 8);
    CHECK(f.get<Transform>("child").previous.position.x == 3);
    CHECK(f.get<Transform>("other").previous.position.x == 0);
    CHECK(f.get<Widget>("other").previous == 2);
    REQUIRE(f.apply(set("other", "consumer.Widget", {{"value", 11}, {"label", "changed"}})));
    CHECK(f.get<Widget>("other").previous == 11);
    REQUIRE(f.apply({{"op", "reparent"}, {"id", "child"}, {"parent", "other"}}));
    CHECK(f.get<Transform>("child").previous.position.x == 3);
    CHECK(f.get<Transform>("other").previous.position.x == 0);
}
TEST_CASE("Save, Play Stop and explicit reload preserve the editor baseline transactionally") {
    namespace fs = std::filesystem;
    Editing f;
    std::ifstream fixture(SOMNIUM_GAME_FIXTURE);
    const std::string text{std::istreambuf_iterator<char>(fixture),
                           std::istreambuf_iterator<char>()};
    auto loaded = deserialize(text, f.codecs, &f.assets);
    REQUIRE(loaded);
    f.world = std::move(*loaded);
    std::ifstream mappingFile(SOMNIUM_GAME_MAPPING);
    const std::string mappingText{std::istreambuf_iterator<char>(mappingFile),
                                  std::istreambuf_iterator<char>()};
    auto mapping = input::Configuration::parse(mappingText);
    REQUIRE(mapping);
    const auto directory =
        fs::temp_directory_path() /
        ("somnium-edit-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(fs::create_directory(directory));
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    } cleanup{directory};
    const auto write = [&](std::string_view value) {
        std::ofstream file(directory / "scene.json", std::ios::binary);
        file << value;
        file.close();
        REQUIRE(file.good());
    };
    write(canonical(*f.world, f.codecs));
    auto files = platform::SourceFiles::create(directory.generic_string());
    REQUIRE(files);
    game::SandboxGame game;
    runtime::IterationSession modes(f.world, game, f.codecs, *mapping, &f.assets);
    runtime::EditSession editor(f.world, modes, f.codecs, *files, "scene.json", &f.assets);
    REQUIRE(editor.initialize());
    REQUIRE(editor.execute(
        set("character", "game.Character", {{"speed", 4}, {"visual", "character-visual"}}), 0));
    CHECK(editor.dirty());
    const auto editing = canonical(*f.world, f.codecs);
    REQUIRE(editor.execute({{"op", "play"}}, 20));
    std::vector<input::ButtonState> buttons(mapping->actions().size());
    std::vector<float> axes(mapping->axes().size());
    axes[mapping->axis("MoveX")->value] = 1;
    for (std::uint64_t step = 20; step != 30; ++step) {
        f.world->beginStep();
        REQUIRE(game.fixedUpdate(*f.world, {1. / 60., step, {step, buttons, axes, {}}}));
        REQUIRE(f.world->propagate());
    }
    CHECK(canonical(*f.world, f.codecs) != editing);
    CHECK_FALSE(editor.execute({{"op", "save"}}, 30));
    CHECK(editor.error().find("Play") != std::string::npos);
    REQUIRE(editor.execute({{"op", "stop"}}, 30));
    CHECK(canonical(*f.world, f.codecs) == editing);
    CHECK(editor.dirty());
    REQUIRE(editor.execute({{"op", "save"}}, 31));
    CHECK_FALSE(editor.dirty());
    REQUIRE(files->read("scene.json"));
    CHECK(*files->read("scene.json") == editing);
    REQUIRE(editor.execute({{"op", "save"}}, 32));
    CHECK(*files->read("scene.json") == editing);
    REQUIRE(editor.execute(set("character", "somnium.Name", {{"value", "Unsaved"}}), 33));
    const auto unsaved = canonical(*f.world, f.codecs);
    CHECK_FALSE(editor.execute({{"op", "reload"}, {"discard", false}}, 34));
    write("{invalid");
    CHECK_FALSE(editor.execute({{"op", "reload"}, {"discard", true}}, 34));
    CHECK(canonical(*f.world, f.codecs) == unsaved);
    CHECK(editor.dirty());
    CHECK_FALSE(editor.execute({{"op", "save"}}, 35));
    CHECK(editor.dirty());
    CHECK(*files->read("scene.json") == "{invalid");
    write(editing);
    REQUIRE(editor.execute({{"op", "reload"}, {"discard", true}}, 36));
    CHECK_FALSE(editor.dirty());
    CHECK(canonical(*f.world, f.codecs) == editing);
    auto restarted = deserialize(*files->read("scene.json"), f.codecs, &f.assets);
    REQUIRE(restarted);
    CHECK(canonical(**restarted, f.codecs) == editing);
}
TEST_CASE("Editor replay rejects malformed, unordered and unversioned command streams") {
    REQUIRE(runtime::parseEditorReplay(
        R"({"version":1,"commands":[{"step":0,"command":{"op":"save"}},{"step":0,"command":{"op":"play"}}]})"));
    for (auto text :
         {R"({"version":2,"commands":[]})",
          R"({"version":1,"commands":[{"step":-1,"command":{}}]})",
          R"({"version":1,"commands":[{"step":2,"command":{}},{"step":1,"command":{}}]})",
          R"({"version":1,"commands":[{"step":0,"command":{},"errorContains":""}]})",
          R"({"version":1,"commands":[],"unknown":true})"})
        CHECK_FALSE(runtime::parseEditorReplay(text));
}
