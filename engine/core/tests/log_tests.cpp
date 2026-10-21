// Tests filtering and formatting through the public facade, without accessing spdlog.
// test_log.py additionally inspects emitted text through a process-level pipe on every OS.
#include <doctest/doctest.h>
#include <somnium/core/log.hpp>

namespace {
struct Reset {
    ~Reset() {
        somnium::Log::setLevel(somnium::Log::Level::Info);
        for (auto category : {"render", "muted"})
            somnium::Log::clearCategoryLevel(category);
    }
};
struct Counted {
    int* calls;
};
} // namespace

template <> struct std::formatter<Counted> : std::formatter<int> {
    auto format(const Counted& value, std::format_context& context) const {
        ++*value.calls;
        return std::formatter<int>::format(7, context);
    }
};

TEST_CASE("Log filters default severity and exact category overrides") {
    using Log = somnium::Log;
    using Level = Log::Level;
    Reset reset;
    Log::setLevel(Level::Warning);
    Log::setCategoryLevel("render", Level::Trace);
    Log::setCategoryLevel("muted", Level::Off);
    CHECK_FALSE(Log::enabled("core", Level::Info));
    CHECK(Log::enabled("core", Level::Warning));
    CHECK(Log::enabled("render", Level::Trace));
    CHECK_FALSE(Log::enabled("renderer", Level::Trace));
    CHECK_FALSE(Log::enabled("muted", Level::Critical));
    Log::write("core", Level::Info, "hidden-global");
    Log::write("core", Level::Warning, "visible-warning");
    Log::write("render", Level::Trace, "visible-category");
    Log::write("renderer", Level::Trace, "hidden-exact-name");
    Log::write("muted", Level::Critical, "hidden-category");
    Log::clearCategoryLevel("render");
    CHECK_FALSE(Log::enabled("render", Level::Info));
    Log::write("render", Level::Info, "hidden-reset");
    Log::write("core", Level::Off, "hidden-off-message");
    Log::setLevel(Level::Off);
    CHECK_FALSE(Log::enabled("core", Level::Critical));
    Log::write("core", Level::Critical, "hidden-all");
    Log::flush();
}

TEST_CASE("Log formats checked arguments only when the message is enabled") {
    using Log = somnium::Log;
    Reset reset;
    Log::setLevel(Log::Level::Error);
    int formats = 0;
    Log::write("core", Log::Level::Info, "{}", Counted{&formats});
    CHECK(formats == 0);
    Log::write("core", Log::Level::Error, "value={} ratio={:.2f} tag={}", Counted{&formats}, 1.25,
               "ok");
    CHECK(formats == 1);
    Log::flush();
}
