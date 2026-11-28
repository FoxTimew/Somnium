// Rejects partial publications, path escapes and content mismatches before touching the device.
// The structural signature is computed from verified layout files, not resource counts alone.
#include "shader_reload.hpp"
#include <algorithm>
#include <format>
#include <nlohmann/json.hpp>
#include <set>
#include <somnium/core/conventions.hpp>
#include <somnium/core/logical_path.hpp>
#include <somnium/core/sha256.hpp>

namespace somnium::renderer::detail {
namespace {
using Json = nlohmann::json;
Error invalid() {
    return {ErrorCode::Io, "Invalid shader program manifest or artifact"};
}
bool text(const Json& value, const char* key) {
    return value.contains(key) && value[key].is_string();
}
bool object(const Json& value, const char* key) {
    return value.contains(key) && value[key].is_object();
}
bool hash(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
bool descriptor(const Json& value) {
    return value.is_object() && value.contains("bytes") && value["bytes"].is_number_unsigned() &&
           value["bytes"].get<std::uint64_t>() > 0 &&
           value["bytes"].get<std::uint64_t>() <= core::kMaxResourceBytes &&
           text(value, "sha256") && hash(value["sha256"].get_ref<const std::string&>());
}
Json parse(std::span<const std::byte> bytes) {
    const auto* begin = reinterpret_cast<const char*>(bytes.data());
    return bytes.empty() ? Json{} : Json::parse(begin, begin + bytes.size(), nullptr, false);
}
std::string sha(std::string_view value) {
    return core::sha256(std::as_bytes(std::span(value.data(), value.size())));
}
bool flat(std::string_view name) {
    return !name.empty() && name != "." && name != ".." &&
           std::all_of(name.begin(), name.end(), [](char c) {
               return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '_' || c == '-' || c == '.';
           });
}
} // namespace
Result<core::Bytes> ShaderProgram::read(std::string_view path) const {
    const auto found = files.find(path);
    if (found == files.end())
        return tl::unexpected(invalid());
    return found->second;
}
Result<ShaderProgram> readProgram(const core::ByteReader& reader, std::string_view manifest,
                                  std::span<const std::byte> document) {
    const auto canonical = core::logicalPath(manifest);
    const auto json = parse(document);
    if (!canonical || *canonical != manifest || !json.is_object() ||
        json.value("version", Json{}) != 1 || json.value("program", Json{}) != "model" ||
        !text(json, "generation") || !text(json, "layout_sha256") || !object(json, "stages") ||
        !object(json, "files") || json["stages"].size() != 3 || json["files"].size() != 18)
        return tl::unexpected(invalid());
    ShaderProgram program;
    program.generation = json["generation"].get<std::string>();
    program.layout = json["layout_sha256"].get<std::string>();
    if (!hash(program.generation) || !hash(program.layout))
        return tl::unexpected(invalid());
    const auto slash = manifest.find_last_of('/');
    const std::string directory(slash == std::string_view::npos ? ""
                                                                : manifest.substr(0, slash + 1));
    const std::string prefix = "model.generations/" + program.generation + "/";
    Json descriptors = Json::object(), records = Json::object();
    std::size_t total = 0;
    for (const auto& [path, value] : json["files"].items()) {
        if (!path.starts_with(prefix) || !flat(std::string_view(path).substr(prefix.size())) ||
            !descriptor(value))
            return tl::unexpected(invalid());
        const auto size = value["bytes"].get<std::size_t>();
        if (size > core::kMaxResourceBytes - total)
            return tl::unexpected(invalid());
        total += size;
        auto bytes = reader.read(directory + path);
        if (!bytes)
            return tl::unexpected(
                Error{ErrorCode::Io, std::format("Shader artifact {}{}: {}", directory, path,
                                                 bytes.error().message)});
        if (bytes->size() != size ||
            core::sha256(*bytes) != value["sha256"].get_ref<const std::string&>())
            return tl::unexpected(
                Error{ErrorCode::Io,
                      std::format("Shader artifact size/SHA-256 mismatch: {}{}", directory, path)});
        descriptors[path.substr(prefix.size())] = value;
        program.files.emplace(directory + path, std::move(*bytes));
    }
    std::set<std::string> referenced;
    std::map<std::string, std::string> layouts;
    const auto fullPrefix = directory + prefix;
    constexpr std::array roles = {"vertex", "lambert", "ambient"};
    for (std::size_t index = 0; index < roles.size(); ++index) {
        const auto* role = roles[index];
        if (!text(json["stages"], role))
            return tl::unexpected(invalid());
        const auto path = json["stages"][role].get<std::string>();
        if (!path.starts_with(prefix) || !json["files"].contains(path))
            return tl::unexpected(invalid());
        const auto stage = parse(program.files.at(directory + path));
        const auto expectedStage = index == 0 ? "vertex" : "fragment";
        const auto expectedEntry = index == 0 ? "vsMain" : "psMain";
        if (!stage.is_object() || stage.value("version", Json{}) != 1 ||
            stage.value("stage", Json{}) != expectedStage ||
            stage.value("entrypoint", Json{}) != expectedEntry || !object(stage, "counts") ||
            !object(stage, "formats") || stage["formats"].size() != 3 ||
            !object(stage, "reflection") || !object(stage, "layout"))
            return tl::unexpected(invalid());
        referenced.insert(path);
        const auto check = [&](const Json& value) {
            if (!descriptor(value) || !text(value, "file"))
                return false;
            const auto name = value["file"].get<std::string>();
            const auto full = prefix + name;
            if (!flat(name) || !json["files"].contains(full))
                return false;
            const auto& known = json["files"][full];
            if (known["bytes"] != value["bytes"] || known["sha256"] != value["sha256"])
                return false;
            return referenced.insert(full).second;
        };
        for (const auto* format : {"spirv", "dxil", "msl"}) {
            if (!object(stage["formats"], format) || !check(stage["formats"][format]) ||
                stage["formats"][format].value("entrypoint", Json{}) != expectedEntry)
                return tl::unexpected(invalid());
        }
        if (!check(stage["reflection"]) || !check(stage["layout"]))
            return tl::unexpected(invalid());
        const auto layout =
            parse(program.files.at(fullPrefix + stage["layout"]["file"].get<std::string>()));
        const auto reflection =
            parse(program.files.at(fullPrefix + stage["reflection"]["file"].get<std::string>()));
        if (!layout.is_object() || !reflection.is_object() ||
            layout.value("version", Json{}) != 1 ||
            layout.value("stage", Json{}) != expectedStage ||
            layout.value("entrypoint", Json{}) != expectedEntry ||
            layout.value("counts", Json{}) != stage["counts"])
            return tl::unexpected(invalid());
        layouts[role] = stage["layout"]["sha256"].get<std::string>();
        records[role] = path.substr(prefix.size());
        program.stages[index] = directory + path;
    }
    if (referenced.size() != program.files.size())
        return tl::unexpected(invalid());
    const Json identity = {{"program", "model"}, {"stages", records}, {"files", descriptors}};
    if (sha(identity.dump(-1, ' ', true) + "\n") != program.generation)
        return tl::unexpected(invalid());
    std::string signature;
    for (const auto& [role, value] : layouts)
        signature.append(role).append(":").append(value).push_back('\n');
    if (sha(signature) != program.layout)
        return tl::unexpected(invalid());
    return program;
}
} // namespace somnium::renderer::detail
