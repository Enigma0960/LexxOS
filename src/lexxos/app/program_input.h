#pragma once

#include <cstdint>

#include <lexxos/devices/input_device.h>
#include <lexxos/runtime/selected_runtime_profile.h>

namespace lexxos::app {

class ProgramInputService;

} // namespace lexxos::app

namespace lexxos::app::program_input {

// Временный адаптер; независимые composition передают собственный сервис.
ProgramInputService& defaultService();

inline constexpr std::uint32_t queueCapacity =
    lexxos::runtime::standardRuntimeProfile.events.programInputQueueCapacity;

struct EventCursor {
	std::uint32_t nextSequence{0};
};

// Terminal duplicates key events into the console byte stream (ANSI-compatible).
// Events delivers one structured InputEvent per physical/UART key, without
// also injecting its bytes into console_input.
enum class InputMode : std::uint8_t {
	Terminal = 0,
	Events = 1,
};

// An input mode belongs to the process, not to the physical keyboard or shell.
// Missing/legacy processes retain Terminal semantics.
[[nodiscard]] bool setInputMode(std::uint32_t processId, InputMode mode);
[[nodiscard]] InputMode inputMode(std::uint32_t processId);
void releaseInputMode(std::uint32_t processId);

// Publish a copy of a physical input event for foreground GUI/full-screen programs.
// This does not consume the hardware FIFO and does not change console_input behavior.
void publish(lexxos::devices::InputEvent const& event);

// Move a program-local cursor to the current end of the queue, dropping stale events.
void reset(EventCursor& cursor);

[[nodiscard]] std::uint32_t sequence();
[[nodiscard]] bool pollEvent(EventCursor& cursor, lexxos::devices::InputEvent& event);
[[nodiscard]] bool latestEvent(lexxos::devices::InputEvent& event);

} // namespace lexxos::app::program_input
