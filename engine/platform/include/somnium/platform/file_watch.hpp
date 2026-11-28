// Observes a logical file at a bounded cadence through the existing data reader.
// Created only for an explicitly enabled iteration service; no OS watcher leaks out.
#pragma once
#include <optional>
#include <somnium/core/byte_reader.hpp>
#include <somnium/core/export.hpp>

namespace somnium::platform {
class FileWatch final {
  public:
    SOMNIUM_ENGINE_API FileWatch(const core::ByteReader& reader, std::string logicalPath);
    // Monotonic seconds are supplied by the owner. An unchanged file/error returns nullopt.
    [[nodiscard]] SOMNIUM_ENGINE_API Result<std::optional<core::Bytes>> poll(double seconds);

  private:
    const core::ByteReader& m_reader;
    std::string m_path, m_digest, m_error;
    double m_next = 0;
};
} // namespace somnium::platform
