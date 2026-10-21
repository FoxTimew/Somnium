// Compiles pinned header-only dependencies under engine exception/RTTI settings.
// Invalid input checks ensure malformed assets and JSON remain recoverable.
#include <doctest/doctest.h>

#include <cgltf.h>
#include <entt/entity/registry.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <nlohmann/json.hpp>
#include <stb_image.h>

#include <string_view>

TEST_CASE("EnTT registry version supports the direct component contract") {
    entt::registry registry;
    const auto entity = registry.create();
    registry.emplace<int>(entity, 42);
    CHECK(registry.get<int>(entity) == 42);
    registry.destroy(entity);
    CHECK_FALSE(registry.valid(entity));
}

TEST_CASE("JSON rejects syntax errors without exceptions and preserves the version") {
    const auto invalid = nlohmann::json::parse("{ broken", nullptr, false);
    CHECK(invalid.is_discarded());
    const auto valid = nlohmann::json::parse(R"({"version":1})", nullptr, false);
    REQUIRE(valid.is_object());
    REQUIRE(valid.contains("version"));
    CHECK(valid["version"].is_number_integer());
    CHECK(valid["version"].get<int>() == 1);
}

TEST_CASE("CPU parsers report invalid memory inputs") {
    constexpr std::string_view invalid = "not a glTF";
    cgltf_options options{};
    cgltf_data* data = nullptr;
    const auto result = cgltf_parse(&options, invalid.data(), invalid.size(), &data);
    CHECK(result != cgltf_result_success);
    cgltf_free(data);
    const unsigned char image[] = {0, 1, 2, 3};
    int width = 0;
    int height = 0;
    int channels = 0;
    auto* pixels = stbi_load_from_memory(image, 4, &width, &height, &channels, 4);
    CHECK(pixels == nullptr);
    stbi_image_free(pixels);
}

TEST_CASE("Right handed zero-to-one projection maps negative camera Z to clip depth") {
    constexpr float nearPlane = 0.1F;
    constexpr float farPlane = 100.0F;
    const auto projection = glm::perspectiveRH_ZO(1.0F, 1.0F, nearPlane, farPlane);
    const auto nearClip = projection * glm::vec4(0.0F, 0.0F, -nearPlane, 1.0F);
    const auto farClip = projection * glm::vec4(0.0F, 0.0F, -farPlane, 1.0F);
    CHECK(nearClip.z / nearClip.w == doctest::Approx(0.0).epsilon(1e-5));
    CHECK(farClip.z / farClip.w == doctest::Approx(1.0).epsilon(1e-5));
}
