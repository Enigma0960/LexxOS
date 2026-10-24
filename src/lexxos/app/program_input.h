#pragma once

#include <cstdint>

#include <lexxos/devices/input_device.h>
#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::app::program_input {

inline constexpr std::uint32_t queueCapacity =
    lexxos::runtime::standardRuntimeProfile.events.programInputQueueCapacity;

struct EventCursor {
	std::uint32_t nextSequence{0};
};

// Publish a copy of a physical input event for foreground GUI/full-screen programs.
// This does not consume the hardware FIFO and does not change console_input behavior.
void publish(lexxos::devices::InputEvent const& event);

// Move a program-local cursor to the current end of the queue, dropping stale events.
void reset(EventCursor& cursor);

[[nodiscard]] std::uint32_t sequence();
[[nodiscard]] bool pollEvent(EventCursor& cursor, lexxos::devices::InputEvent& event);
[[nodiscard]] bool latestEvent(lexxos::devices::InputEvent& event);

} // namespace lexxos::app::program_input
