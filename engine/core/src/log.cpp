// Routes owned-engine diagnostics directly to a thread-safe spdlog console sink.
// A sink avoids exporting logger ownership or shared_ptr through the facade.
#include <somnium/core/log.hpp>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace somnium {
namespace {
spdlog::sinks::stdout_color_sink_mt& consoleSink() {
    static spdlog::sinks::stdout_color_sink_mt sink;
    return sink;
}
} // namespace

void Log::write(std::string_view category, Level level, std::string_view message) {
    const auto severity = [&] {
        switch (level) {
        case Level::Trace:
            return spdlog::level::trace;
        case Level::Debug:
            return spdlog::level::debug;
        case Level::Info:
            return spdlog::level::info;
        case Level::Warning:
            return spdlog::level::warn;
        case Level::Error:
            return spdlog::level::err;
        case Level::Critical:
            return spdlog::level::critical;
        }
        return spdlog::level::critical;
    }();
    consoleSink().log(
        spdlog::details::log_msg(spdlog::string_view_t(category.data(), category.size()), severity,
                                 spdlog::string_view_t(message.data(), message.size())));
}

void Log::flush() {
    consoleSink().flush();
}
} // namespace somnium
