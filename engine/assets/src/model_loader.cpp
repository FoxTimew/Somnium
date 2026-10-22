// Imports the pinned glTF subset into CPU values; all external reads use ByteReader.
// Optional features produce one aggregate warning, while malformed required data fails.
#include <algorithm>
#include <cgltf.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <set>
#include <somnium/assets/model.hpp>
#include <somnium/core/log.hpp>
#include <somnium/core/profile.hpp>

namespace somnium::assets {
namespace {
Error invalid(std::string message) {
    return {ErrorCode::InvalidArgument, std::move(message)};
}
// NOLINTBEGIN(bugprone-macro-parentheses)
#define SOMNIUM_ASSET_TRY(name, expression)                                                        \
    auto name = (expression);                                                                      \
    if (!name)                                                                                     \
    return tl::unexpected(name.error())
// NOLINTEND(bugprone-macro-parentheses)
struct ReadContext {
    const core::ByteReader& reader;
    std::optional<Error> error;
};
cgltf_result readBuffer(const cgltf_memory_options*, const cgltf_file_options* options,
                        const char* path, cgltf_size* size, void** output) {
    auto& context = *static_cast<ReadContext*>(options->user_data);
    auto name = core::logicalPath(path);
    if (!name) {
        context.error = name.error();
        return cgltf_result_io_error;
    }
    auto bytes = context.reader.read(*name);
    if (!bytes) {
        context.error = bytes.error();
        return cgltf_result_io_error;
    }
    const auto length = *size ? *size : bytes->size();
    if (length > bytes->size() || length > core::kMaxResourceBytes)
        return cgltf_result_data_too_short;
    void* data = std::malloc(length);
    if (!data)
        return cgltf_result_out_of_memory;
    std::memcpy(data, bytes->data(), length);
    *size = length;
    *output = data;
    return cgltf_result_success;
}
void releaseBuffer(const cgltf_memory_options*, const cgltf_file_options*, void* data) {
    std::free(data);
}
int hexDigit(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}
struct ResourceUri {
    std::string_view model;
    std::string_view uri;
};
Result<std::string> resourcePath(ResourceUri resource) {
    const auto model = resource.model;
    const auto uri = resource.uri;
    std::string decoded;
    for (std::size_t i = 0; i < uri.size(); ++i) {
        if (uri[i] != '%') {
            decoded += uri[i];
            continue;
        }
        if (i + 2 >= uri.size() || hexDigit(uri[i + 1]) < 0 || hexDigit(uri[i + 2]) < 0)
            return tl::unexpected(invalid("Malformed resource URI"));
        decoded += static_cast<char>(hexDigit(uri[i + 1]) * 16 + hexDigit(uri[i + 2]));
        i += 2;
    }
    if (decoded.empty() || decoded.front() == '/' ||
        decoded.find_first_of("\\:\0", 0, 3) != std::string::npos)
        return tl::unexpected(invalid("External resource URI is not relative"));
    const auto slash = model.rfind('/');
    return core::logicalPath(
        std::string(model.substr(0, slash == std::string_view::npos ? 0 : slash + 1)) + decoded);
}
Result<core::Bytes> dataUri(std::string_view uri) {
    const auto comma = uri.find(',');
    if (comma == std::string_view::npos || !uri.substr(0, comma).ends_with(";base64"))
        return tl::unexpected(invalid("Only base64 data URIs are supported"));
    const auto encoded = uri.substr(comma + 1);
    if (encoded.empty() || encoded.size() % 4)
        return tl::unexpected(invalid("Invalid base64 size"));
    const auto padding = encoded.ends_with("==") ? 2U : encoded.ends_with('=') ? 1U : 0U;
    const auto length = encoded.size() / 4 * 3 - padding;
    if (!length || length > core::kMaxResourceBytes)
        return tl::unexpected(invalid("Oversized data URI"));
    const auto alphabet = [](char value) {
        return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
               (value >= '0' && value <= '9') || value == '+' || value == '/';
    };
    if (!std::all_of(encoded.begin(), encoded.end() - padding, alphabet))
        return tl::unexpected(invalid("Invalid base64 character"));
    cgltf_options options{};
    void* pointer = nullptr;
    const std::string terminated(encoded);
    if (cgltf_load_buffer_base64(&options, length, terminated.c_str(), &pointer) !=
        cgltf_result_success)
        return tl::unexpected(invalid("Invalid base64 content"));
    std::unique_ptr<void, decltype(&std::free)> owned(pointer, std::free);
    const auto* begin = static_cast<const std::byte*>(pointer);
    return core::Bytes(begin, begin + length);
}
Result<void> validateLayout(cgltf_data& data) {
    std::uint64_t total = 0;
    for (const auto& buffer : std::span(data.buffers, data.buffers_count)) {
        if (!buffer.size || buffer.size > core::kMaxResourceBytes ||
            total > core::kMaxResourceBytes - buffer.size)
            return tl::unexpected(invalid("Invalid glTF buffer budget"));
        total += buffer.size;
    }
    for (const auto& view : std::span(data.buffer_views, data.buffer_views_count)) {
        if (!view.buffer || view.offset > view.buffer->size ||
            view.size > view.buffer->size - view.offset)
            return tl::unexpected(invalid("glTF buffer view out of bounds"));
    }
    for (const auto& accessor : std::span(data.accessors, data.accessors_count)) {
        if (accessor.is_sparse)
            return tl::unexpected(invalid("Sparse accessors are unsupported"));
        const auto* view = accessor.buffer_view;
        const auto element = cgltf_calc_size(accessor.type, accessor.component_type);
        const auto alignment = cgltf_component_size(accessor.component_type);
        if (!view || !accessor.count || !element || !alignment || accessor.stride < element ||
            (view->offset + accessor.offset) % alignment || accessor.stride % alignment ||
            accessor.offset > view->size || element > view->size - accessor.offset ||
            accessor.count - 1 > (view->size - accessor.offset - element) / accessor.stride)
            return tl::unexpected(invalid("glTF accessor out of bounds or misaligned"));
    }
    // cgltf_node_transform_world follows parent pointers; bound them before invoking it.
    for (const auto& node : std::span(data.nodes, data.nodes_count)) {
        const auto* parent = &node;
        for (std::size_t depth = 0; parent; ++depth, parent = parent->parent) {
            if (depth > data.nodes_count || depth > 1024)
                return tl::unexpected(invalid("Cyclic or excessively deep glTF hierarchy"));
        }
    }
    return {};
}
Filter filter(cgltf_filter_type value) {
    return value == cgltf_filter_type_nearest ||
                   value == cgltf_filter_type_nearest_mipmap_nearest ||
                   value == cgltf_filter_type_nearest_mipmap_linear
               ? Filter::Nearest
               : Filter::Linear;
}
struct Import {
    const core::ByteReader& reader;
    std::string_view path;
    cgltf_data& data;
    Model model;
    std::set<std::string> ignored;
    std::vector<std::uint32_t> imageMap;
    std::vector<std::uint32_t> materialMap;
    std::uint64_t decodedBytes = 0;
    bool budget(std::uint64_t bytes) {
        if (bytes > core::kMaxResourceBytes || decodedBytes > core::kMaxResourceBytes - bytes)
            return false;
        decodedBytes += bytes;
        return true;
    }
    Result<std::uint32_t> image(const cgltf_image& source) {
        const auto sourceIndex = static_cast<std::size_t>(&source - data.images);
        if (imageMap[sourceIndex] != core::kInvalidHandleIndex)
            return imageMap[sourceIndex];
        ImageLoader loader;
        Result<Image> result = tl::unexpected(invalid("Image has no URI or buffer view"));
        if (source.buffer_view) {
            const auto* bytes = cgltf_buffer_view_data(source.buffer_view);
            if (!bytes)
                return tl::unexpected(invalid("Image buffer is absent"));
            result = loader.decode(
                {reinterpret_cast<const std::byte*>(bytes), source.buffer_view->size});
        } else if (source.uri) {
            const std::string_view uri(source.uri);
            if (uri.starts_with("data:")) {
                SOMNIUM_ASSET_TRY(bytes, dataUri(uri));
                result = loader.decode(*bytes);
            } else {
                SOMNIUM_ASSET_TRY(name, resourcePath({.model = path, .uri = uri}));
                result = loader.load(reader, *name);
            }
        }
        if (!result)
            return tl::unexpected(result.error());
        if (!budget(result->rgba.size()))
            return tl::unexpected(invalid("Decoded model exceeds budget"));
        const auto index = static_cast<std::uint32_t>(model.images.size());
        model.images.push_back(std::move(*result));
        imageMap[sourceIndex] = index;
        return index;
    }
    Result<std::uint32_t> material(const cgltf_material* source) {
        if (!source)
            return 0;
        const auto sourceIndex = static_cast<std::size_t>(source - data.materials);
        if (materialMap[sourceIndex] != core::kInvalidHandleIndex)
            return materialMap[sourceIndex];
        Material result;
        if (source->alpha_mode != cgltf_alpha_mode_opaque || source->double_sided)
            ignored.insert("alpha/doubleSided");
        if (source->normal_texture.texture || source->occlusion_texture.texture ||
            source->emissive_texture.texture ||
            source->pbr_metallic_roughness.metallic_roughness_texture.texture)
            ignored.insert("non-baseColor textures");
        if (source->has_pbr_metallic_roughness) {
            const auto& pbr = source->pbr_metallic_roughness;
            std::copy_n(pbr.base_color_factor, 4, result.baseColor.begin());
            if (!std::all_of(result.baseColor.begin(), result.baseColor.end(), [](float value) {
                    return std::isfinite(value) && value >= 0 && value <= 1;
                }))
                return tl::unexpected(invalid("Invalid baseColor factor"));
            const auto& view = pbr.base_color_texture;
            if (view.texture && view.texcoord != 0)
                ignored.insert("baseColor UV set != 0");
            if (view.has_transform)
                ignored.insert("texture transform");
            if (view.texture && view.texcoord == 0) {
                if (!view.texture->image)
                    return tl::unexpected(invalid("Required baseColor image absent"));
                SOMNIUM_ASSET_TRY(loaded, image(*view.texture->image));
                result.image = *loaded;
                if (const auto* sampler = view.texture->sampler) {
                    result.sampler = {
                        filter(sampler->min_filter), filter(sampler->mag_filter),
                        sampler->wrap_s == cgltf_wrap_mode_clamp_to_edge ? AddressMode::Clamp
                                                                         : AddressMode::Repeat,
                        sampler->wrap_t == cgltf_wrap_mode_clamp_to_edge ? AddressMode::Clamp
                                                                         : AddressMode::Repeat};
                    if (sampler->wrap_s == cgltf_wrap_mode_mirrored_repeat ||
                        sampler->wrap_t == cgltf_wrap_mode_mirrored_repeat)
                        ignored.insert("mirrored repeat -> repeat");
                }
            }
        }
        const auto index = static_cast<std::uint32_t>(model.materials.size());
        model.materials.push_back(result);
        materialMap[sourceIndex] = index;
        return index;
    }
    Result<Primitive> primitive(const cgltf_primitive& source, PrimitiveIdentity identity) {
        const auto* position = cgltf_find_accessor(&source, cgltf_attribute_type_position, 0);
        const auto* normal = cgltf_find_accessor(&source, cgltf_attribute_type_normal, 0);
        const auto* uv = cgltf_find_accessor(&source, cgltf_attribute_type_texcoord, 0);
        const auto* index = source.indices;
        if (source.type != cgltf_primitive_type_triangles || !position || !index ||
            index->count % 3 ||
            (index->component_type != cgltf_component_type_r_16u &&
             index->component_type != cgltf_component_type_r_32u) ||
            position->type != cgltf_type_vec3 ||
            position->component_type != cgltf_component_type_r_32f ||
            (normal && (normal->type != cgltf_type_vec3 || normal->count != position->count)) ||
            (uv && (uv->type != cgltf_type_vec2 || uv->count != position->count)))
            return tl::unexpected(
                invalid("Required indexed triangle POSITION/NORMAL/UV data invalid"));
        if (source.targets_count)
            ignored.insert("morph targets");
        for (const auto& attribute : std::span(source.attributes, source.attributes_count)) {
            if (attribute.type != cgltf_attribute_type_position &&
                attribute.type != cgltf_attribute_type_normal &&
                !(attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0))
                ignored.insert("optional vertex attributes");
        }
        const auto vertexCount = normal ? position->count : index->count;
        if (!budget(vertexCount * sizeof(Vertex) + index->count * sizeof(std::uint32_t)))
            return tl::unexpected(invalid("Decoded model exceeds budget"));
        Primitive result;
        result.identity = identity;
        SOMNIUM_ASSET_TRY(materialIndex, material(source.material));
        result.material = *materialIndex;
        std::vector<std::uint32_t> indices(index->count);
        if (cgltf_accessor_unpack_indices(index, indices.data(), sizeof(std::uint32_t),
                                          indices.size()) != indices.size())
            return tl::unexpected(invalid("Cannot decode indices"));
        if (std::any_of(indices.begin(), indices.end(),
                        [&](auto value) { return value >= position->count; }))
            return tl::unexpected(invalid("Index outside POSITION accessor"));
        std::vector<Vertex> vertices(position->count);
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            auto& vertex = vertices[i];
            if (!cgltf_accessor_read_float(position, i, vertex.position.data(), 3) ||
                (normal && !cgltf_accessor_read_float(normal, i, vertex.normal.data(), 3)) ||
                (uv && !cgltf_accessor_read_float(uv, i, vertex.uv.data(), 2)))
                return tl::unexpected(invalid("Cannot decode vertex attributes"));
            const auto finite = [](float value) { return std::isfinite(value); };
            if (!std::all_of(vertex.position.begin(), vertex.position.end(), finite) ||
                !std::all_of(vertex.normal.begin(), vertex.normal.end(), finite) ||
                !std::all_of(vertex.uv.begin(), vertex.uv.end(), finite))
                return tl::unexpected(invalid("Non-finite vertex data"));
        }
        if (!uv)
            ignored.insert("missing UV0 -> zero");
        if (!normal) {
            ignored.insert("missing normals -> face normals");
            result.vertices.reserve(indices.size());
            for (std::size_t i = 0; i < indices.size(); i += 3) {
                const auto a = glm::make_vec3(vertices[indices[i]].position.data());
                const auto b = glm::make_vec3(vertices[indices[i + 1]].position.data());
                const auto c = glm::make_vec3(vertices[indices[i + 2]].position.data());
                auto face = glm::cross(b - a, c - a);
                face = glm::dot(face, face) > 0 ? glm::normalize(face) : glm::vec3(0, 1, 0);
                for (std::size_t j = 0; j < 3; ++j) {
                    auto vertex = vertices[indices[i + j]];
                    std::copy_n(glm::value_ptr(face), 3, vertex.normal.begin());
                    result.vertices.push_back(vertex);
                    indices[i + j] = static_cast<std::uint32_t>(i + j);
                }
            }
        } else
            result.vertices = std::move(vertices);
        if (index->component_type == cgltf_component_type_r_16u &&
            result.vertices.size() <= 65536) {
            std::vector<std::uint16_t> compact;
            compact.reserve(indices.size());
            for (auto value : indices)
                compact.push_back(static_cast<std::uint16_t>(value));
            result.indices = std::move(compact);
        } else
            result.indices = std::move(indices);
        return result;
    }
};
} // namespace
Result<Model> ModelLoader::load(const core::ByteReader& reader, std::string_view path) const {
    SOMNIUM_PROFILE_SCOPE("assets.gltf");
    SOMNIUM_ASSET_TRY(name, core::logicalPath(path));
    SOMNIUM_ASSET_TRY(bytes, reader.read(*name));
    if (bytes->empty() || bytes->size() > core::kMaxResourceBytes)
        return tl::unexpected(invalid("Invalid model size"));
    ReadContext context{reader, {}};
    cgltf_options options{};
    options.file = {readBuffer, releaseBuffer, &context};
    cgltf_data* parsed = nullptr;
    if (cgltf_parse(&options, bytes->data(), bytes->size(), &parsed) != cgltf_result_success)
        return tl::unexpected(invalid("Cannot parse glTF 2.0"));
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(parsed, cgltf_free);
    if (data->extensions_required_count)
        return tl::unexpected(invalid("Required glTF extension unsupported"));
    SOMNIUM_ASSET_TRY(layout, validateLayout(*data));
    for (const auto& buffer : std::span(data->buffers, data->buffers_count)) {
        if (buffer.uri && !std::string_view(buffer.uri).starts_with("data:")) {
            SOMNIUM_ASSET_TRY(uri, resourcePath({.model = *name, .uri = buffer.uri}));
        }
    }
    if (cgltf_load_buffers(&options, data.get(), name->c_str()) != cgltf_result_success)
        return tl::unexpected(context.error.value_or(invalid("Cannot load glTF buffers")));
    for (const auto& buffer : std::span(data->buffers, data->buffers_count))
        if (!buffer.data)
            return tl::unexpected(invalid("Required buffer data absent"));
    if (cgltf_validate(data.get()) != cgltf_result_success)
        return tl::unexpected(invalid("Invalid glTF data"));
    Import importer{reader,
                    *name,
                    *data,
                    {},
                    {},
                    std::vector<std::uint32_t>(data->images_count, core::kInvalidHandleIndex),
                    std::vector<std::uint32_t>(data->materials_count, core::kInvalidHandleIndex),
                    0};
    importer.model.images.push_back(
        {1, 1, {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}});
    importer.model.materials.emplace_back();
    if (data->extensions_used_count)
        importer.ignored.insert("optional extensions");
    if (data->skins_count || data->animations_count || data->cameras_count || data->lights_count)
        importer.ignored.insert("skins/animations/cameras/lights");
    std::vector<std::uint32_t> firstPrimitive(data->meshes_count);
    for (std::size_t mesh = 0; mesh < data->meshes_count; ++mesh) {
        firstPrimitive[mesh] = static_cast<std::uint32_t>(importer.model.primitives.size());
        const auto& source = data->meshes[mesh];
        for (std::size_t primitive = 0; primitive < source.primitives_count; ++primitive) {
            SOMNIUM_ASSET_TRY(loaded, importer.primitive(source.primitives[primitive],
                                                         {static_cast<std::uint32_t>(mesh),
                                                          static_cast<std::uint32_t>(primitive)}));
            importer.model.primitives.push_back(std::move(*loaded));
        }
    }
    std::vector<const cgltf_node*> pending;
    const auto* scene = data->scene ? data->scene : data->scenes_count ? &data->scenes[0] : nullptr;
    if (scene) {
        for (auto* node : std::span(scene->nodes, scene->nodes_count))
            pending.push_back(node);
    } else {
        for (const auto& node : std::span(data->nodes, data->nodes_count))
            if (!node.parent)
                pending.push_back(&node);
    }
    while (!pending.empty()) {
        const auto* node = pending.back();
        pending.pop_back();
        for (auto* child : std::span(node->children, node->children_count))
            pending.push_back(child);
        if (!node->mesh)
            continue;
        glm::mat4 world{1};
        cgltf_node_transform_world(node, glm::value_ptr(world));
        if (!std::all_of(glm::value_ptr(world), glm::value_ptr(world) + 16,
                         [](float value) { return std::isfinite(value); }))
            return tl::unexpected(invalid("Non-finite node matrix"));
        const auto mesh = static_cast<std::size_t>(node->mesh - data->meshes);
        for (std::size_t primitive = 0; primitive < node->mesh->primitives_count; ++primitive)
            importer.model.instances.push_back(
                {firstPrimitive[mesh] + static_cast<std::uint32_t>(primitive), world});
    }
    for (const auto& feature : importer.ignored) {
        if (!importer.model.warning.empty())
            importer.model.warning += "; ";
        importer.model.warning += feature;
    }
    if (!importer.model.warning.empty())
        Log::write("assets", Log::Level::Warning, "{}: ignored/fallback: {} (opaque, BACK culling)",
                   *name, importer.model.warning);
    return std::move(importer.model);
}
} // namespace somnium::assets
