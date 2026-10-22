// Keeps the native event translation private to platform.
// The public event stream contains only Somnium enums and raw values.
#pragma once
#include <SDL3/SDL_events.h>
#include <optional>
#include <somnium/core/input_event.hpp>

namespace somnium::platform::detail {
[[nodiscard]] std::optional<core::InputEvent> translateInput(const SDL_Event& event,
                                                             std::uint64_t step);
}
