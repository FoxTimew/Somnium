// Exercises real glTF/PNG fixtures and deliberately malformed variants through memory I/O.
// No window, GPU, platform implementation or process working directory is required.
#include <algorithm>
#include <doctest/doctest.h>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <somnium/assets/model.hpp>

namespace {
using namespace somnium;
using namespace somnium::assets;
using Json = nlohmann::json;
struct Reader final : core::ByteReader {
    std::map<std::string, core::Bytes, std::less<>> files;
    mutable std::vector<std::string> reads;
    Result<core::Bytes> read(std::string_view name) const override {
        reads.emplace_back(name);
        const auto found = files.find(name);
        if (found == files.end())
            return tl::unexpected(Error{ErrorCode::NotFound, "Missing fixture"});
        return found->second;
    }
    void fixture(std::string_view name) {
        std::ifstream input(std::string(SOMNIUM_ASSET_FIXTURES) + "/" + std::string(name),
                            std::ios::binary);
        const std::vector<char> data{std::istreambuf_iterator<char>(input),
                                     std::istreambuf_iterator<char>()};
        const auto bytes = std::as_bytes(std::span(data));
        files[std::string(name)] = {bytes.begin(), bytes.end()};
    }
    void json(const Json& data) {
        const auto text = data.dump();
        const auto bytes = std::as_bytes(std::span(text));
        files["model.gltf"] = {bytes.begin(), bytes.end()};
    }
};
Reader fixtures() {
    Reader reader;
    for (const auto* name : {"model.gltf", "model.glb", "embedded.gltf", "mesh.bin", "texture.png"})
        reader.fixture(name);
    return reader;
}
Json source(const Reader& reader) {
    const auto& bytes = reader.files.at("model.gltf");
    return Json::parse(reinterpret_cast<const char*>(bytes.data()),
                       reinterpret_cast<const char*>(bytes.data()) + bytes.size());
}
} // namespace
TEST_CASE(
    "glTF and GLB preserve primitive identity, index widths, pixels and flattened transforms") {
    auto reader = fixtures();
    ModelLoader loader;
    for (const auto* path : {"model.gltf", "model.glb", "embedded.gltf"}) {
        auto result = loader.load(reader, path);
        REQUIRE(result);
        REQUIRE(result->primitives.size() == 3);
        CHECK(result->primitives[0].identity == PrimitiveIdentity{0, 0});
        CHECK(result->primitives[1].identity == PrimitiveIdentity{0, 1});
        CHECK(result->primitives[2].identity == PrimitiveIdentity{1, 0});
        CHECK(std::holds_alternative<std::vector<std::uint16_t>>(result->primitives[0].indices));
        CHECK(std::holds_alternative<std::vector<std::uint32_t>>(result->primitives[1].indices));
        CHECK(std::get<std::vector<std::uint16_t>>(result->primitives[0].indices) ==
              std::vector<std::uint16_t>{0, 1, 2});
        CHECK(std::get<std::vector<std::uint32_t>>(result->primitives[1].indices) ==
              std::vector<std::uint32_t>{0, 1, 2});
        REQUIRE(result->primitives[0].vertices.size() == 3);
        CHECK(result->primitives[0].vertices[1].position == std::array<float, 3>{1, 0, 0});
        CHECK(result->primitives[0].vertices[0].normal == std::array<float, 3>{0, 0, 1});
        CHECK(result->primitives[0].vertices[2].uv == std::array<float, 2>{0, 1});
        CHECK(result->primitives[0].material == result->primitives[1].material);
        CHECK(result->primitives[2].material == 0);
        REQUIRE(result->images.size() == 2);
        CHECK(result->images[1].width == 2);
        CHECK(result->images[1].rgba[0] == std::byte{255});
        CHECK(result->images[1].rgba[2] == std::byte{0});
        CHECK(result->images[1].rgba[10] == std::byte{255}); // Bottom left blue: no vertical flip.
        const auto& material = result->materials[result->primitives[0].material];
        CHECK(material.baseColor == std::array<float, 4>{0.5F, 0.25F, 1, 1});
        CHECK(material.sampler.min == Filter::Nearest);
        CHECK(material.sampler.mag == Filter::Linear);
        CHECK(material.sampler.u == AddressMode::Clamp);
        CHECK(material.sampler.v == AddressMode::Repeat);
        REQUIRE(result->instances.size() == 3);
        for (const auto& instance : result->instances)
            if (instance.primitive == 0) {
                CHECK(instance.world[3][0] == doctest::Approx(1));
                CHECK(instance.world[3][1] == doctest::Approx(6));
                CHECK(instance.world[3][2] == doctest::Approx(3));
                CHECK(instance.world[0][1] == doctest::Approx(2));
                CHECK(instance.world[1][0] == doctest::Approx(-3));
                CHECK(instance.world[2][2] == doctest::Approx(4));
            }
        CHECK(result->warning.empty());
    }
}
TEST_CASE("asset states separate accepted request from failure and stale handles") {
    auto reader = fixtures();
    ModelLoader loader;
    AssetManager<Model> manager(reader, loader);
    auto handle = manager.identify("models/../model.gltf");
    REQUIRE(handle);
    CHECK(*manager.state(*handle) == State::Unloaded);
    CHECK_FALSE(manager.get(*handle));
    REQUIRE(manager.request(*handle));
    CHECK(*manager.state(*handle) == State::Ready);
    REQUIRE(manager.get(*handle));
    const auto reads = reader.reads.size();
    CHECK(*manager.request("./model.gltf") == *handle);
    CHECK(reader.reads.size() == reads);
    REQUIRE(manager.unload(*handle));
    CHECK_FALSE(manager.state(*handle));
    CHECK_FALSE(manager.get(*handle));
    auto reused = manager.request("model.gltf");
    REQUIRE(reused);
    CHECK(reused->index == handle->index);
    CHECK(reused->generation != handle->generation);
    auto failed = manager.request("absent.gltf");
    REQUIRE(failed);
    CHECK(*manager.state(*failed) == State::Failed);
    CHECK(manager.error(*failed)->code == ErrorCode::NotFound);
    CHECK_FALSE(manager.request("../outside"));
}

TEST_CASE("external URIs resolve relative to the model and decode percent escapes") {
    auto reader = fixtures();
    auto json = source(reader);
    json["buffers"][0]["uri"] = "../mesh.bin";
    json["images"][0]["uri"] = "../texture%20space.png";
    reader.json(json);
    reader.files["models/model.gltf"] = reader.files.at("model.gltf");
    reader.files["texture space.png"] = reader.files.at("texture.png");
    ModelLoader loader;
    auto model = loader.load(reader, "models/model.gltf");
    REQUIRE(model);
    CHECK(model->images.size() == 2);
    CHECK(std::find(reader.reads.begin(), reader.reads.end(), "texture space.png") !=
          reader.reads.end());
}
TEST_CASE("Loading is observable and cannot be unloaded reentrantly") {
    Reader reader;
    struct ProbeLoader final : Loader<int> {
        std::function<void()> duringLoad;
        Result<int> load(const core::ByteReader&, std::string_view) const override {
            duringLoad();
            return 7;
        }
    } loader;
    AssetManager<int> manager(reader, loader);
    auto handle = manager.identify("number");
    REQUIRE(handle);
    loader.duringLoad = [&] {
        CHECK(*manager.state(*handle) == State::Loading);
        CHECK_FALSE(manager.get(*handle));
        CHECK_FALSE(manager.unload(*handle));
        CHECK(manager.request(*handle));
    };
    REQUIRE(manager.request(*handle));
    CHECK(manager.get(*handle)->get() == 7);
}
TEST_CASE("optional material and vertex features warn once and preserve opaque model data") {
    auto reader = fixtures();
    auto json = source(reader);
    json["materials"][0]["alphaMode"] = "BLEND";
    json["materials"][0]["doubleSided"] = true;
    json["materials"][0]["normalTexture"] = {{"index", 1}};
    json["images"].push_back({{"uri", "ignored-missing.png"}});
    json["textures"].push_back({{"source", 1}});
    json["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"] = 0;
    json["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_1"] = 2;
    reader.json(json);
    ModelLoader loader;
    AssetManager<Model> manager(reader, loader);
    auto handle = manager.request("model.gltf");
    REQUIRE(handle);
    REQUIRE(*manager.state(*handle) == State::Ready);
    auto model = manager.get(*handle);
    REQUIRE(model);
    CHECK_FALSE(model->get().warning.empty());
    CHECK(model->get().primitives.size() == 3);
    CHECK(std::find(reader.reads.begin(), reader.reads.end(), "ignored-missing.png") ==
          reader.reads.end());
    const auto reads = reader.reads.size();
    REQUIRE(manager.request("model.gltf"));
    CHECK(reader.reads.size() == reads);
}
TEST_CASE("absent normals and UV0 get deterministic fallbacks") {
    auto reader = fixtures();
    auto json = source(reader);
    auto& attributes = json["meshes"][0]["primitives"][0]["attributes"];
    attributes.erase("NORMAL");
    attributes.erase("TEXCOORD_0");
    reader.json(json);
    ModelLoader loader;
    auto model = loader.load(reader, "model.gltf");
    REQUIRE(model);
    CHECK(model->primitives[0].vertices[0].normal == std::array<float, 3>{0, 0, 1});
    CHECK(model->primitives[0].vertices[0].uv == std::array<float, 2>{0, 0});
}
TEST_CASE("required or malformed glTF data fails explicitly") {
    const auto original = fixtures();
    ModelLoader loader;
    const std::array<std::function<void(Json&)>, 8> mutations{
        [](Json& json) { json["meshes"][0]["primitives"][0]["attributes"].erase("POSITION"); },
        [](Json& json) { json["meshes"][0]["primitives"][0]["mode"] = 1; },
        [](Json& json) { json["extensionsRequired"] = {"UNSUPPORTED_test"}; },
        [](Json& json) {
            json["accessors"][0]["sparse"] = {
                {"count", 1},
                {"indices", {{"bufferView", 1}, {"componentType", 5123}}},
                {"values", {{"bufferView", 0}}}};
        },
        [](Json& json) { json["accessors"][0]["count"] = 1000000000; },
        [](Json& json) { json["buffers"][0]["uri"] = "../outside.bin"; },
        [](Json& json) { json["buffers"][0]["uri"] = "mesh.bin%00extra"; },
        [](Json& json) { json["buffers"][0]["byteLength"] = 9999; }};
    for (const auto& mutate : mutations) {
        auto reader = original;
        auto json = source(reader);
        mutate(json);
        reader.json(json);
        auto result = loader.load(reader, "model.gltf");
        CHECK_FALSE(result);
        if (!result)
            CHECK_FALSE(result.error().message.empty());
    }
    auto reader = original;
    reader.files.erase("texture.png");
    auto missing = loader.load(reader, "model.gltf");
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == ErrorCode::NotFound);
    reader = original;
    reader.files["mesh.bin"][96] = std::byte{99};
    CHECK_FALSE(loader.load(reader, "model.gltf"));
    reader.files["model.glb"].resize(20);
    CHECK_FALSE(loader.load(reader, "model.glb"));
    ImageLoader image;
    CHECK_FALSE(image.decode({}));
    CHECK_FALSE(image.decode(reader.files["mesh.bin"]));
}
