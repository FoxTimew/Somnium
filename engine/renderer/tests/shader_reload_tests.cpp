// Builds small complete publications in memory and checks the consumer without a device.
// Hash, path and schema failures cannot leak mutable source data into a verified generation.
#include "shader_reload.hpp"
#include <doctest/doctest.h>
#include <nlohmann/json.hpp>
#include <somnium/core/sha256.hpp>

namespace {
using namespace somnium;
using Json = nlohmann::json;
core::Bytes bytes(std::string_view text) {
    const auto data = std::as_bytes(std::span(text.data(), text.size()));
    return {data.begin(), data.end()};
}
core::Bytes encoded(const Json& json) {
    return bytes(json.dump() + "\n");
}
struct Reader final : core::ByteReader {
    std::map<std::string, core::Bytes, std::less<>> files;
    Result<core::Bytes> read(std::string_view path) const override {
        const auto found = files.find(path);
        if (found == files.end())
            return tl::unexpected(Error{ErrorCode::Io, "Missing test artifact"});
        return found->second;
    }
};
struct Publication {
    Reader reader;
    Json document;
    Publication() {
        Json files = Json::object(), stages = Json::object();
        std::map<std::string, core::Bytes> contents;
        std::string signature;
        const auto add = [&](const std::string& name, core::Bytes value) {
            Json descriptor = {{"bytes", value.size()}, {"sha256", core::sha256(value)}};
            files[name] = descriptor;
            contents[name] = std::move(value);
            descriptor["file"] = name;
            return descriptor;
        };
        for (const std::string role : {"ambient", "lambert", "vertex"}) {
            const auto stage = role == "vertex" ? "vertex" : "fragment";
            const auto entry = role == "vertex" ? "vsMain" : "psMain";
            const Json counts = {{"samplers", 1},
                                 {"uniform_buffers", 2},
                                 {"storage_buffers", 0},
                                 {"storage_textures", 0}};
            Json manifest = {
                {"version", 1}, {"stage", stage}, {"entrypoint", entry}, {"counts", counts}};
            for (const std::string format : {"spirv", "dxil", "msl"}) {
                auto value =
                    add(std::string(role).append(".").append(format), bytes("test artifact"));
                value["entrypoint"] = entry;
                manifest["formats"][format] = std::move(value);
            }
            manifest["reflection"] = add(role + ".reflect.json", encoded(Json::object()));
            manifest["layout"] = add(
                role + ".layout.json",
                encoded(
                    {{"version", 1}, {"stage", stage}, {"entrypoint", entry}, {"counts", counts}}));
            signature.append(role)
                .append(":")
                .append(manifest["layout"]["sha256"].get<std::string>())
                .push_back('\n');
            stages[role] = role + ".json";
            (void)add(role + ".json", encoded(manifest));
        }
        const auto generation =
            core::sha256(encoded({{"program", "model"}, {"stages", stages}, {"files", files}}));
        const auto prefix = "model.generations/" + generation + "/";
        document = {{"version", 1},
                    {"program", "model"},
                    {"generation", generation},
                    {"layout_sha256", core::sha256(bytes(signature))}};
        for (const auto& [role, path] : stages.items())
            document["stages"][role] = prefix + path.get<std::string>();
        const auto fullPrefix = "shaders/" + prefix;
        for (const auto& [name, value] : files.items()) {
            document["files"][prefix + name] = value;
            reader.files[fullPrefix + name] = std::move(contents.at(name));
        }
    }
    auto load() const {
        return renderer::detail::readProgram(reader, "shaders/model.program.json",
                                             encoded(document));
    }
};
} // namespace
TEST_CASE("Shader program snapshots all verified bytes") {
    Publication publication;
    auto program = publication.load();
    REQUIRE(program);
    CHECK(program->files.size() == 18);
    const auto stage = program->read(program->stages[0]);
    REQUIRE(stage);
    publication.reader.files.clear();
    CHECK(program->read(program->stages[0]).value() == *stage);
    CHECK_FALSE(publication.load());
}
TEST_CASE("Shader publication rejects schema paths hashes and incomplete generations") {
    for (const auto* member : {"version", "generation", "stages", "files", "layout_sha256"}) {
        Publication publication;
        publication.document[member] = Json::array();
        CHECK_FALSE(publication.load());
    }
    Publication corrupt;
    corrupt.reader.files.begin()->second.push_back(std::byte{0});
    CHECK_FALSE(corrupt.load());
    Publication unsafe;
    const auto descriptor = unsafe.document["files"].begin().value();
    unsafe.document["files"].erase(unsafe.document["files"].begin());
    unsafe.document["files"]["../escape"] = descriptor;
    CHECK_FALSE(unsafe.load());
    Publication signature;
    signature.document["layout_sha256"] = std::string(64, '0');
    CHECK_FALSE(signature.load());
    Publication wrongStage;
    wrongStage.document["stages"]["vertex"] = wrongStage.document["stages"]["ambient"];
    CHECK_FALSE(wrongStage.load());
}
