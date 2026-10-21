// Routes owned-engine diagnostics directly to a thread-safe spdlog console sink.
// A sink avoids exporting logger ownership or shared_ptr through the facade.
#include <somnium/core/log.hpp>

#include <map>
#include <mutex>
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>

namespace somnium {
namespace {
struct Settings {
    std::mutex mutex;
    Log::Level minimum = Log::Level::Info;
    std::map<std::string, Log::Level, std::less<>> categories;
};
Settings& settings() {
    static Settings state;
    return state;
}
spdlog::sinks::stdout_color_sink_mt& consoleSink() {
    static spdlog::sinks::stdout_color_sink_mt sink;
    return sink;
}
} // namespace

void Log::setLevel(Level minimum) {
    auto& state = settings();
    const std::lock_guard lock(state.mutex);
    state.minimum = minimum;
}
void Log::setCategoryLevel(std::string_view category, Level minimum) {
    auto& state = settings();
    const std::lock_guard lock(state.mutex);
    state.categories.insert_or_assign(std::string(category), minimum);
}
void Log::clearCategoryLevel(std::string_view category) {
    auto& state = settings();
    const std::lock_guard lock(state.mutex);
    const auto found = state.categories.find(category);
    if (found != state.categories.end())
        state.categories.erase(found);
}
bool Log::enabled(std::string_view category, Level level) {
    if (level == Level::Off)
        return false;
    auto& state = settings();
    const std::lock_guard lock(state.mutex);
    const auto found = state.categories.find(category);
    const auto minimum = found == state.categories.end() ? state.minimum : found->second;
    return minimum != Level::Off && level >= minimum;
}

void Log::write(std::string_view category, Level level, std::string_view message) {
    if (!enabled(category, level))
        return;
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
        case Level::Off:
            return spdlog::level::off;
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
