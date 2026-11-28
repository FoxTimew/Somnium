// Uses a counting reader and an explicit clock to test iteration polling without sleeping.
// Missing files, recovery and identical-size replacements must not hide a new publication.
#include <doctest/doctest.h>
#include <somnium/platform/file_watch.hpp>
namespace {
struct Reader final : somnium::core::ByteReader {
    mutable int reads = 0;
    bool fail = false;
    somnium::core::Bytes value{std::byte{1}};
    somnium::Result<somnium::core::Bytes> read(std::string_view) const override {
        ++reads;
        if (fail)
            return tl::unexpected(somnium::Error{somnium::ErrorCode::Io, "Missing publication"});
        return value;
    }
};
} // namespace
TEST_CASE("File watch limits I/O and observes recovery") {
    Reader reader;
    somnium::platform::FileWatch watch(reader, "published.json");
    CHECK(reader.reads == 0);
    auto change = watch.poll(0);
    REQUIRE(change);
    CHECK(change->has_value());
    for (int i = 0; i < 25; ++i) {
        change = watch.poll(static_cast<double>(i) / 100);
        REQUIRE(change);
        CHECK_FALSE(change->has_value());
    }
    CHECK(reader.reads == 1);
    reader.value[0] = std::byte{2};
    change = watch.poll(0.25);
    REQUIRE(change);
    const auto& publication = *change;
    if (publication.has_value())
        CHECK((*publication)[0] == std::byte{2});
    else
        FAIL("Changed bytes were not published");
    reader.fail = true;
    CHECK_FALSE(watch.poll(0.5));
    change = watch.poll(0.75);
    REQUIRE(change);
    CHECK_FALSE(change->has_value());
    reader.fail = false;
    change = watch.poll(1);
    REQUIRE(change);
    CHECK(change->has_value());
    CHECK_FALSE(watch.poll(-1));
}
