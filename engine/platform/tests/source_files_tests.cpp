// Exercises real source publication and its last validation boundary without a window.
// Temporary roots isolate failure injection from authored repository scenes.
#include "source_files_atomic.hpp"
#include <chrono>
#include <doctest/doctest.h>
#include <fstream>
#include <somnium/platform/source_files.hpp>

namespace {
namespace fs = std::filesystem;
using namespace somnium;
struct Files {
    fs::path root;
    Files() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = fs::temp_directory_path() / ("somnium-source-" + std::to_string(stamp));
        REQUIRE(fs::create_directory(root));
        fs::create_directory(root / "scenes");
        write("scenes/a.json", "old\n");
    }
    ~Files() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    void write(const fs::path& name, std::string_view text) {
        std::ofstream file(root / name, std::ios::binary);
        file << text;
        file.close();
        REQUIRE(file.good());
    }
    std::string read(std::string_view name) {
        std::ifstream file(root / name, std::ios::binary);
        REQUIRE(file.good());
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
    std::size_t files() const {
        return static_cast<std::size_t>(
            std::distance(fs::directory_iterator(root / "scenes"), fs::directory_iterator{}));
    }
};
} // namespace
TEST_CASE("Source publication is byte stable and refuses an externally changed baseline") {
    Files files;
    auto source = platform::SourceFiles::create(files.root.generic_string());
    REQUIRE(source);
    REQUIRE(source->replace("scenes/a.json", {.expected = "old\n", .contents = "canonical\n"}));
    CHECK(files.read("scenes/a.json") == "canonical\n");
    CHECK(files.files() == 1);
    const auto stamp = fs::last_write_time(files.root / "scenes/a.json");
    REQUIRE(
        source->replace("scenes/a.json", {.expected = "canonical\n", .contents = "canonical\n"}));
    CHECK(fs::last_write_time(files.root / "scenes/a.json") == stamp);
    files.write("scenes/a.json", "external\n");
    auto refused =
        source->replace("scenes/a.json", {.expected = "canonical\n", .contents = "overwrite\n"});
    REQUIRE_FALSE(refused);
    CHECK(refused.error().message.find("externally") != std::string::npos);
    CHECK(files.read("scenes/a.json") == "external\n");
    CHECK(files.files() == 1);
}
TEST_CASE("A failed final validation leaves the old complete file and removes the temporary") {
    Files files;
    auto result = platform::detail::atomicReplace(
        files.root / "scenes/a.json", std::string(std::size_t{1024} * 1024, 'n'), 7,
        [&]() -> Result<void> {
            CHECK(files.files() == 2);
            CHECK(files.read("scenes/a.json") == "old\n");
            return tl::unexpected(Error{ErrorCode::Io, "Injected failure before publication"});
        });
    REQUIRE_FALSE(result);
    CHECK(files.read("scenes/a.json") == "old\n");
    CHECK(files.files() == 1);
    auto changed = platform::detail::atomicReplace(
        files.root / "scenes/a.json", "candidate", 8, [&]() -> Result<void> {
            files.write("scenes/a.json", "concurrent writer");
            return tl::unexpected(Error{ErrorCode::InvalidState, "External modification"});
        });
    CHECK_FALSE(changed);
    CHECK(files.read("scenes/a.json") == "concurrent writer");
    CHECK(files.files() == 1);
}
TEST_CASE("Source roots reject traversal, non-files and escaping symlinks") {
    Files files, outside;
    auto source = platform::SourceFiles::create(files.root.generic_string() + "/");
    REQUIRE(source);
    REQUIRE(source->read("scenes/a.json"));
    for (auto path : {"../outside.json", "/absolute", "C:/native", "scenes/../a.json",
                      "scenes//a.json", "scenes/a.json/"}) {
        CHECK_FALSE(source->read(path));
        CHECK_FALSE(source->replace(path, {.expected = "", .contents = "bad"}));
    }
    CHECK_FALSE(source->replace("scenes/missing.json", {.expected = "", .contents = "new"}));
    CHECK_FALSE(source->replace("scenes", {.expected = "", .contents = "bad"}));
    CHECK_FALSE(platform::SourceFiles::create("relative"));
    CHECK_FALSE(platform::SourceFiles::create((files.root / "absent").generic_string()));
    std::error_code error;
    fs::create_directory_symlink(outside.root, files.root / "escape", error);
    if (!error) {
        CHECK_FALSE(source->read("escape/scenes/a.json"));
        CHECK_FALSE(
            source->replace("escape/scenes/a.json", {.expected = "old\n", .contents = "bad"}));
        CHECK(outside.read("scenes/a.json") == "old\n");
    } else {
        MESSAGE("Directory symlink permission unavailable: ", error.message());
    }
    CHECK(files.read("scenes/a.json") == "old\n");
}
