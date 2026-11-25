// Verifies injected file access and diagnostics without creating a window.
// Data is resolved from a test root rather than the process working directory.
#include <array>
#include <doctest/doctest.h>
#include <somnium/platform/file_reader.hpp>

TEST_CASE("FileReader preserves bytes and reports missing resources") {
    auto reader = somnium::platform::FileReader::create(SOMNIUM_TEST_DATA_DIR);
    REQUIRE(reader);
    auto data = reader->read("sample.txt");
    REQUIRE(data);
    CHECK(data->size() > 0);
    CHECK(data->front() == std::byte{'#'});
    auto missing = reader->read("does_not_exist.txt");
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == somnium::ErrorCode::NotFound);
    CHECK_FALSE(missing.error().message.empty());
}

TEST_CASE("ordered roots prefer the first match and fall back only for absence") {
    const std::string first = std::string(SOMNIUM_TEST_DATA_DIR) + "/first";
    const std::string second = std::string(SOMNIUM_TEST_DATA_DIR) + "/second";
    const std::array<std::string_view, 2> roots{first, second};
    auto reader = somnium::platform::FileReader::createOrdered(roots);
    REQUIRE(reader);
    auto preferred = reader->read("priority.txt");
    REQUIRE(preferred);
    CHECK(preferred->front() == std::byte{'1'});
    auto fallback = reader->read("fallback.txt");
    REQUIRE(fallback);
    CHECK(fallback->front() == std::byte{'2'});
    auto directory = reader->read("blocked.txt");
    REQUIRE_FALSE(directory);
    CHECK(directory.error().code == somnium::ErrorCode::Io);
    const std::string unusable = first + "/priority.txt/not-a-directory";
    const std::array<std::string_view, 2> lazyRoots{first, unusable};
    auto lazy = somnium::platform::FileReader::createOrdered(lazyRoots);
    REQUIRE(lazy);
    CHECK(lazy->read("priority.txt")); // A lower-priority root is never probed unnecessarily.
    auto trailing = somnium::platform::FileReader::create(first + "/");
    REQUIRE(trailing);
    CHECK(trailing->read("priority.txt"));
}

TEST_CASE("default shipping root cannot fall back to development source assets") {
    auto reader = somnium::platform::FileReader::create();
    REQUIRE(reader);
    auto source = reader->read("data_root_probe.txt");
#if SOMNIUM_TEST_DEVELOPMENT_DATA
    CHECK(source.has_value());
#else
    REQUIRE_FALSE(source);
    CHECK(source.error().code == somnium::ErrorCode::NotFound);
#endif
}

TEST_CASE("Logical paths reject native and parent traversal forms") {
    auto reader = somnium::platform::FileReader::create(SOMNIUM_TEST_DATA_DIR);
    REQUIRE(reader);
    for (const auto* path : {"", "../secret", "x/../secret", "/absolute", "C:/native", "x\\y",
                             "./sample.txt", "x//y", "sample.txt/"}) {
        auto result = reader->read(path);
        REQUIRE_FALSE(result);
        CHECK(result.error().code == somnium::ErrorCode::InvalidArgument);
    }
    CHECK_FALSE(reader->read(std::string_view("x\0y", 3)));
}
