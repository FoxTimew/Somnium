// Exercises public error and identity contracts independently of SDL.
// Reuse tests check the stale-handle failure that resource consumers must rely on.
#include <doctest/doctest.h>
#include <somnium/core/assert.hpp>
#include <somnium/core/handle.hpp>
#include <somnium/core/logical_path.hpp>
#include <somnium/core/profile.hpp>
#include <somnium/core/result.hpp>

#include <memory>
#include <type_traits>

namespace {
struct MeshTag {};
struct TextureTag {};
static_assert(sizeof(somnium::Handle<MeshTag>) == 8);
static_assert(!std::is_convertible_v<somnium::Handle<MeshTag>, somnium::Handle<TextureTag>>);
} // namespace

TEST_CASE("logical identities normalize relative segments without escaping a root") {
    CHECK(*somnium::core::logicalPath("models/../textures/./image.png") == "textures/image.png");
    for (const auto* name :
         {"", "..", "a/../../outside", "/absolute", "a//b", "a/", "C:/file", "a\\b"})
        CHECK_FALSE(somnium::core::logicalPath(name));
    CHECK_FALSE(somnium::core::logicalPath(std::string_view("x\0y", 3)));
    CHECK(somnium::core::logicalPath("textures/été.png"));
    CHECK_FALSE(somnium::core::logicalPath("textures/\xc0\xaf.png"));
    CHECK_FALSE(somnium::core::validUtf8("\xed\xa0\x80"));
}

TEST_CASE("Result preserves successful move-only values and owned failure diagnostics") {
    somnium::Result<std::unique_ptr<int>> value = std::make_unique<int>(42);
    REQUIRE(value.has_value());
    auto moved = std::move(value);
    CHECK(**moved == 42);
    somnium::Result<void> success;
    CHECK(success.has_value());
    somnium::Result<int> failure =
        tl::unexpected(somnium::Error{somnium::ErrorCode::Io, std::string("missing file")});
    REQUIRE_FALSE(failure.has_value());
    CHECK(failure.error().code == somnium::ErrorCode::Io);
    CHECK(failure.error().message == "missing file");
}

TEST_CASE("Released identities stay invalid when their slot is reused") {
    somnium::HandlePool<MeshTag> pool;
    CHECK_FALSE(static_cast<bool>(somnium::Handle<MeshTag>{}));
    CHECK_FALSE(pool.contains({}));
    auto first = pool.acquire();
    auto second = pool.acquire();
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->index != second->index);
    CHECK(pool.contains(*first));
    REQUIRE(pool.release(*first));
    CHECK_FALSE(pool.contains(*first));
    CHECK_FALSE(pool.release(*first));
    auto replacement = pool.acquire();
    REQUIRE(replacement);
    CHECK(replacement->index == first->index);
    CHECK(replacement->generation != first->generation);
    CHECK_FALSE(pool.contains(*first));
    CHECK(pool.contains(*second));
    CHECK(pool.contains(*replacement));
    CHECK_FALSE(pool.release({second->index, second->generation + 1}));
    CHECK(pool.contains(*second));
}

TEST_CASE("Verification evaluates once and disabled instrumentation has no side effects") {
    int calls = 0;
    SOMNIUM_VERIFY(++calls == 1);
    CHECK(calls == 1);
#ifdef NDEBUG
    SOMNIUM_ASSERT(++calls == 2);
    CHECK(calls == 1);
#endif
#ifndef SOMNIUM_ENABLE_PROFILING
    SOMNIUM_PROFILE_ALLOC(nullptr, ++calls);
    SOMNIUM_PROFILE_FREE(++calls);
    CHECK(calls == 1);
#endif
}
