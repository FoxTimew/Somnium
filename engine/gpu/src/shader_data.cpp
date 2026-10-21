// Validates versioned offline manifests without exceptions or native file paths.
// The injected reader supplies bytes; only the device-supported artifact is consumed.
#include "shader_data.hpp"
#include <nlohmann/json.hpp>
#include <somnium/core/conventions.hpp>

#include <algorithm>
#include <cstring>

namespace somnium::gpu::detail {
namespace {
Error invalidManifest() {
    return {ErrorCode::Io, "Invalid shader manifest or artifact"};
}
bool unsignedField(const nlohmann::json& object, const char* name, std::uint64_t maximum) {
    const auto value = object.find(name);
    return value != object.end() && value->is_number_unsigned() &&
           value->get<std::uint64_t>() <= maximum;
}
bool textField(const nlohmann::json& object, const char* name) {
    const auto value = object.find(name);
    return value != object.end() && value->is_string();
}
} // namespace

Result<ShaderData> readShader(const core::ByteReader& reader, std::string_view manifest,
                              ShaderFormat format) {
    auto input = reader.read(manifest);
    if (!input) {
        return tl::unexpected(input.error());
    }
    if (input->empty()) {
        return tl::unexpected(invalidManifest());
    }
    const auto* begin = reinterpret_cast<const char*>(input->data());
    const auto json = nlohmann::json::parse(begin, begin + input->size(), nullptr, false);
    if (!json.is_object() || !unsignedField(json, "version", 1) || json["version"] != 1 ||
        !textField(json, "stage") || !textField(json, "entrypoint") || !json.contains("counts") ||
        !json["counts"].is_object() || !json.contains("formats") || !json["formats"].is_object()) {
        return tl::unexpected(invalidManifest());
    }
    ShaderStage stage;
    if (json["stage"] == "vertex" && json["entrypoint"] == "vsMain") {
        stage = ShaderStage::Vertex;
    } else if (json["stage"] == "fragment" && json["entrypoint"] == "psMain") {
        stage = ShaderStage::Fragment;
    } else {
        return tl::unexpected(invalidManifest());
    }
    const char* key;
    const char* extension;
    switch (format) {
    case ShaderFormat::Spirv:
        key = "spirv";
        extension = ".spv";
        break;
    case ShaderFormat::Dxil:
        key = "dxil";
        extension = ".dxil";
        break;
    case ShaderFormat::Msl:
        key = "msl";
        extension = ".msl";
        break;
    default:
        return tl::unexpected(invalidManifest());
    }
    const auto& counts = json["counts"];
    if (!unsignedField(counts, "samplers", 16) || !unsignedField(counts, "storage_textures", 8) ||
        !unsignedField(counts, "storage_buffers", 8) ||
        !unsignedField(counts, "uniform_buffers", 4)) {
        return tl::unexpected(invalidManifest());
    }
    const auto& formats = json["formats"];
    if (!formats.contains(key) || !formats[key].is_object()) {
        return tl::unexpected(invalidManifest());
    }
    const auto& artifact = formats[key];
    if (!textField(artifact, "file") || !textField(artifact, "entrypoint") ||
        artifact["entrypoint"] != json["entrypoint"] ||
        !unsignedField(artifact, "bytes", core::kMaxResourceBytes) || artifact["bytes"] == 0) {
        return tl::unexpected(invalidManifest());
    }
    const auto file = artifact["file"].get<std::string>();
    if (file.empty() || file.find_first_of("/\\:") != std::string::npos ||
        file.find('\0') != std::string::npos || !file.ends_with(extension)) {
        return tl::unexpected(invalidManifest());
    }
    const auto separator = manifest.find_last_of('/');
    const std::string directory(separator == std::string_view::npos
                                    ? std::string_view{}
                                    : manifest.substr(0, separator + 1));
    auto bytes = reader.read(directory + file);
    if (!bytes) {
        return tl::unexpected(bytes.error());
    }
    if (bytes->size() != artifact["bytes"].get<std::size_t>()) {
        return tl::unexpected(invalidManifest());
    }
    if ((format == ShaderFormat::Spirv &&
         (bytes->size() < 20 || bytes->size() % 4 ||
          std::memcmp(bytes->data(), "\x03\x02\x23\x07", 4) != 0)) ||
        (format == ShaderFormat::Dxil &&
         (bytes->size() < 32 || std::memcmp(bytes->data(), "DXBC", 4) != 0)) ||
        (format == ShaderFormat::Msl &&
         std::find(bytes->begin(), bytes->end(), std::byte{0}) != bytes->end())) {
        return tl::unexpected(invalidManifest());
    }
    return ShaderData{std::move(*bytes),
                      stage,
                      format,
                      json["entrypoint"].get<std::string>(),
                      counts["samplers"].get<std::uint32_t>(),
                      counts["storage_textures"].get<std::uint32_t>(),
                      counts["storage_buffers"].get<std::uint32_t>(),
                      counts["uniform_buffers"].get<std::uint32_t>()};
}
} // namespace somnium::gpu::detail

namespace somnium::gpu {
Result<Shader> Device::loadShader(const core::ByteReader& reader, std::string_view manifest) {
    auto data = detail::readShader(reader, manifest, info().shaderFormat);
    if (!data) {
        return tl::unexpected(data.error());
    }
    return createShader(data->description());
}
} // namespace somnium::gpu
