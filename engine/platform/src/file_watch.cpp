// Polling hashes content, so atomic replacement and equal timestamps remain observable.
// Repeated failures are suppressed until the file changes or becomes readable again.
#include <cmath>
#include <somnium/core/sha256.hpp>
#include <somnium/platform/file_watch.hpp>
#include <utility>

namespace somnium::platform {
FileWatch::FileWatch(const core::ByteReader& reader, std::string logicalPath)
    : m_reader(reader), m_path(std::move(logicalPath)) {}
Result<std::optional<core::Bytes>> FileWatch::poll(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0)
        return tl::unexpected(Error{ErrorCode::InvalidArgument, "Invalid watch clock"});
    if (seconds < m_next)
        return std::optional<core::Bytes>{};
    m_next = seconds + 0.25;
    auto bytes = m_reader.read(m_path);
    if (!bytes) {
        if (m_error == bytes.error().message)
            return std::optional<core::Bytes>{};
        m_error = bytes.error().message;
        m_digest.clear();
        return tl::unexpected(bytes.error());
    }
    m_error.clear();
    auto digest = core::sha256(*bytes);
    if (digest == m_digest)
        return std::optional<core::Bytes>{};
    m_digest = std::move(digest);
    return std::optional<core::Bytes>(std::move(*bytes));
}
} // namespace somnium::platform
