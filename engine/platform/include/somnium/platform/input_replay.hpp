// Replays neutral events at exact fixed-step boundaries without an OS event queue.
// Parsing and delivery are CPU-only; each step must be requested exactly once.
#pragma once
#include <somnium/core/byte_reader.hpp>
#include <somnium/core/input_event.hpp>
#include <span>
namespace somnium::platform {
class InputReplay final {
  public:
    [[nodiscard]] static Result<InputReplay> parse(std::string_view text);
    [[nodiscard]] static Result<InputReplay> load(const core::ByteReader& reader,
                                                  std::string_view path);
    [[nodiscard]] Result<std::span<const core::InputEvent>> next(std::uint64_t step);

  private:
    std::vector<core::InputEvent> m_events;
    std::size_t m_cursor = 0;
    std::uint64_t m_nextStep = 0;
};
} // namespace somnium::platform
