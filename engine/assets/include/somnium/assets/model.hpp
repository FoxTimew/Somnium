// Describes immutable imported CPU data without GPU handles or native graphics types.
// A primitive keeps its file-local identity; node instances carry flattened world matrices.
#pragma once
#include <array>
#include <glm/mat4x4.hpp>
#include <somnium/assets/manager.hpp>
#include <span>
#include <variant>

namespace somnium::assets {
enum class Filter : std::uint8_t { Nearest, Linear };
enum class AddressMode : std::uint8_t { Repeat, Clamp };
struct SamplerDescription {
    Filter min = Filter::Linear;
    Filter mag = Filter::Linear;
    AddressMode u = AddressMode::Repeat;
    AddressMode v = AddressMode::Repeat;
};
struct Image {
    std::uint32_t width = 0, height = 0;
    core::Bytes rgba;
};
struct Vertex {
    std::array<float, 3> position;
    std::array<float, 3> normal;
    std::array<float, 2> uv;
};
static_assert(sizeof(Vertex) == 32 && offsetof(Vertex, normal) == 12 && offsetof(Vertex, uv) == 24);
using Indices = std::variant<std::vector<std::uint16_t>, std::vector<std::uint32_t>>;
struct PrimitiveIdentity {
    std::uint32_t mesh, primitive;
    friend bool operator==(PrimitiveIdentity, PrimitiveIdentity) = default;
};
struct Primitive {
    PrimitiveIdentity identity;
    std::vector<Vertex> vertices;
    Indices indices;
    std::uint32_t material = 0;
};
struct Material {
    std::array<float, 4> baseColor{1, 1, 1, 1};
    std::uint32_t image = 0;
    SamplerDescription sampler;
};
struct Instance {
    std::uint32_t primitive;
    glm::mat4 world{1};
};
struct Model {
    std::vector<Primitive> primitives;
    std::vector<Material> materials;
    std::vector<Image> images;
    std::vector<Instance> instances;
    std::string warning;
};
class ImageLoader final : public Loader<Image> {
  public:
    [[nodiscard]] Result<Image> load(const core::ByteReader& reader,
                                     std::string_view path) const override;
    [[nodiscard]] Result<Image> decode(std::span<const std::byte> bytes) const;
};
class ModelLoader final : public Loader<Model> {
  public:
    [[nodiscard]] Result<Model> load(const core::ByteReader& reader,
                                     std::string_view path) const override;
};
} // namespace somnium::assets
