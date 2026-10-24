#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/input/detail/input_event_queue.h>
#include <lexxos/devices/input/detail/picocalc_keyboard_input_state.h>
#include <lexxos/devices/input_device.h>

namespace lexxos::devices::input_detail {

class PicoCalcKeyboardEventBridge final {
public:
	static constexpr std::size_t eventQueueCapacity = 32;

	void clear();
	void setRawEventReporting(bool enabled);

	[[nodiscard]] bool pollEvent(lexxos::devices::InputEvent& event);
	[[nodiscard]] bool hasPendingEvents() const;
	[[nodiscard]] bool hasActiveInput() const;
	[[nodiscard]] bool isPressed(lexxos::devices::KeyCode key) const;
	[[nodiscard]] std::uint32_t rawState() const;
	[[nodiscard]] std::uint32_t staleScanCodeReleases() const;
	[[nodiscard]] std::uint32_t staleReleaseCount() const;

	void handleRawEvent(std::uint8_t status, std::uint8_t code, std::uint32_t now);
	void updateRepeatEvents(std::uint32_t now);
	void releaseStaleScanCodes(std::uint32_t now);

private:
	void _acceptInputResult(PicoCalcKeyboardInputResult const& result);

	PicoCalcKeyboardInputState _inputState{};
	InputEventQueue<eventQueueCapacity> _events{};
	std::uint32_t _lastRawEvent{0};
	bool _rawEventReporting{false};
};

} // namespace lexxos::devices::input_detail
