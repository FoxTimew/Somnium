// Checks manifest failures through an injected in-memory reader, without requiring a GPU.
// Synthetic headers test structural validation; only D5 proves driver acceptance of real shaders.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "shader_data.hpp"
#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <map>
#include <string>

namespace {
using namespace somnium;
using namespace somnium::gpu;
using Json = nlohmann::json;
struct MemoryReader final : core::ByteReader {
    std::map<std::string, core::Bytes> files;
    Result<core::Bytes> read(std::string_view name) const override {
        const auto found = files.find(std::string(name));
        if (found == files.end()) {
            return tl::unexpected(Error{ErrorCode::Io, "Missing test data"});
        }
        return found->second;
    }
    void text(const std::string& name, const std::string& value) {
        const auto* begin = reinterpret_cast<const std::byte*>(value.data());
        files[name] = {begin, begin + value.size()};
    }
};
struct Fixture {
    MemoryReader reader;
    Json manifest{{"version", 1},
                  {"stage", "vertex"},
                  {"entrypoint", "vsMain"},
                  {"counts",
                   {{"samplers", 0},
                    {"storage_textures", 0},
                    {"storage_buffers", 0},
                    {"uniform_buffers", 0}}},
                  {"formats",
                   {{"spirv", {{"file", "shader.spv"}, {"bytes", 20}, {"entrypoint", "vsMain"}}},
                    {"dxil", {{"file", "shader.dxil"}, {"bytes", 32}, {"entrypoint", "vsMain"}}},
                    {"msl", {{"file", "shader.msl"}, {"bytes", 4}, {"entrypoint", "vsMain"}}}}}};
    Fixture() {
        reader.files["shaders/shader.spv"] = core::Bytes(20);
        auto& spirv = reader.files["shaders/shader.spv"];
        spirv[0] = std::byte{3};
        spirv[1] = std::byte{2};
        spirv[2] = std::byte{0x23};
        spirv[3] = std::byte{7};
        reader.text("shaders/shader.dxil", std::string("DXBC") + std::string(28, '\0'));
        reader.text("shaders/shader.msl", "text");
    }
    Result<detail::ShaderData> load(ShaderFormat format = ShaderFormat::Spirv) {
        reader.text("shaders/shader.json", manifest.dump());
        return detail::readShader(reader, "shaders/shader.json", format);
    }
};
} // namespace

TEST_CASE("Shader format selection preserves owned bytes and metadata") {
    Fixture fixture;
    for (const auto format : {ShaderFormat::Spirv, ShaderFormat::Dxil, ShaderFormat::Msl}) {
        auto result = fixture.load(format);
        REQUIRE(result);
        CHECK(result->format == format);
        CHECK(result->stage == ShaderStage::Vertex);
        CHECK(result->entrypoint == "vsMain");
        CHECK(result->description().code.data() == result->bytes.data());
    }
    fixture.manifest["stage"] = "fragment";
    fixture.manifest["entrypoint"] = "psMain";
    fixture.manifest["formats"]["spirv"]["entrypoint"] = "psMain";
    fixture.manifest["counts"]["samplers"] = 1;
    auto fragment = fixture.load();
    REQUIRE(fragment);
    CHECK(fragment->stage == ShaderStage::Fragment);
    CHECK(fragment->samplers == 1);
}

TEST_CASE("Malformed JSON and missing reads return an error without exceptions") {
    MemoryReader reader;
    CHECK_FALSE(detail::readShader(reader, "missing", ShaderFormat::Spirv));
    for (const auto* invalid : {"", "{", "null", "[]", "{}", "true"}) {
        reader.text("bad", invalid);
        auto result = detail::readShader(reader, "bad", ShaderFormat::Spirv);
        REQUIRE_FALSE(result);
        CHECK(result.error().code == ErrorCode::Io);
    }
}

TEST_CASE("Manifest schema, entries, resource counts and selected format are bounded") {
    Fixture fixture;
    const auto original = fixture.manifest;
    for (const auto* field : {"version", "stage", "entrypoint", "counts", "formats"}) {
        fixture.manifest = original;
        fixture.manifest.erase(field);
        CHECK_FALSE(fixture.load());
    }
    fixture.manifest = original;
    fixture.manifest["version"] = 2;
    CHECK_FALSE(fixture.load());
    fixture.manifest = original;
    fixture.manifest["entrypoint"] = "main";
    CHECK_FALSE(fixture.load());
    fixture.manifest = original;
    for (const auto& value : {Json(-1), Json(17), Json(1.5), Json("1"), Json(true)}) {
        fixture.manifest["counts"]["samplers"] = value;
        CHECK_FALSE(fixture.load());
    }
    fixture.manifest = original;
    fixture.manifest["formats"].erase("spirv");
    CHECK_FALSE(fixture.load());
    CHECK(fixture.load(ShaderFormat::Dxil));
}

TEST_CASE("Artifact traversal, missing files, mismatched sizes and headers fail") {
    Fixture fixture;
    for (const auto* name :
         {"../shader.spv", "C:/shader.spv", "folder\\shader.spv", "shader.dxil"}) {
        fixture.manifest["formats"]["spirv"]["file"] = name;
        CHECK_FALSE(fixture.load());
    }
    fixture.manifest["formats"]["spirv"]["file"] = "shader.spv";
    fixture.manifest["formats"]["spirv"]["bytes"] = 19;
    CHECK_FALSE(fixture.load());
    fixture.manifest["formats"]["spirv"]["bytes"] = 20;
    fixture.reader.files["shaders/shader.spv"][0] = std::byte{0};
    CHECK_FALSE(fixture.load());
    fixture.reader.files.erase("shaders/shader.spv");
    CHECK_FALSE(fixture.load());
    fixture.reader.files["shaders/shader.dxil"][0] = std::byte{0};
    CHECK_FALSE(fixture.load(ShaderFormat::Dxil));
    fixture.reader.files["shaders/shader.msl"][0] = std::byte{0};
    CHECK_FALSE(fixture.load(ShaderFormat::Msl));
}
