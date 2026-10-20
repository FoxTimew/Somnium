// Verifies injected file access and diagnostics without creating a window.
// Data is resolved from a test root rather than the process working directory.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
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
    CHECK(missing.error().code == somnium::ErrorCode::Io);
    CHECK_FALSE(missing.error().message.empty());
}

TEST_CASE("Logical paths reject native and parent traversal forms") {
    auto reader = somnium::platform::FileReader::create(SOMNIUM_TEST_DATA_DIR);
    REQUIRE(reader);
    for (const auto* path : {"", "../secret", "x/../secret", "/absolute", "C:/native", "x\\y",
                             "./sample.txt", "x//y"}) {
        auto result = reader->read(path);
        REQUIRE_FALSE(result);
        CHECK(result.error().code == somnium::ErrorCode::InvalidArgument);
    }
    CHECK_FALSE(reader->read(std::string_view("x\0y", 3)));
}
