// Decodes PNG/JPEG in memory with a bounded RGBA8 result and unchanged row order.
// stb has no file access; all external bytes come from the injected reader.
#include <climits>
#include <memory>
#include <somnium/assets/model.hpp>
#include <somnium/core/profile.hpp>
#include <stb_image.h>

namespace somnium::assets {
Result<Image> ImageLoader::load(const core::ByteReader& reader, std::string_view path) const {
    auto bytes = reader.read(path);
    if (!bytes)
        return tl::unexpected(bytes.error());
    return decode(*bytes);
}
Result<Image> ImageLoader::decode(std::span<const std::byte> bytes) const {
    SOMNIUM_PROFILE_SCOPE("assets.image");
    if (bytes.empty() || bytes.size() > core::kMaxResourceBytes || bytes.size() > INT_MAX)
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid image byte count"});
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const auto length = static_cast<int>(bytes.size());
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(data, length, &width, &height, &channels) || width <= 0 ||
        height <= 0 || width > 16384 || height > 16384 ||
        std::uint64_t(width) * std::uint64_t(height) * 4 > core::kMaxResourceBytes)
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid or oversized PNG/JPEG"});
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
        stbi_load_from_memory(data, length, &width, &height, &channels, 4), stbi_image_free);
    if (!decoded)
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Cannot decode PNG/JPEG"});
    const auto* begin = reinterpret_cast<const std::byte*>(decoded.get());
    return Image{static_cast<std::uint32_t>(width),
                 static_cast<std::uint32_t>(height),
                 {begin, begin + std::size_t(width) * std::size_t(height) * 4}};
}
} // namespace somnium::assets
