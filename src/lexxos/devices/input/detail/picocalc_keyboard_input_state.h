#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/input/detail/picocalc_scan_code_state.h>
#include <lexxos/devices/input_device.h>

namespace lexxos::devices::input_detail {

struct PicoCalcKeyboardInputResult {
	bool rawEventAccepted{false};
	std::uint32_t rawEvent{0};
	bool hasEvent{false};
	lexxos::devices::InputEvent event{};
};

class PicoCalcKeyboardInputState final {
public:
	static constexpr std::size_t scanCodeCount = PicoCalcScanCodeState::scanCodeCount;

	void clear();

	[[nodiscard]] bool isPressed(lexxos::devices::KeyCode key) const;
	[[nodiscard]] bool hasActiveInput() const;
	[[nodiscard]] std::uint32_t staleScanCodeReleases() const;
	[[nodiscard]] std::uint32_t staleReleaseCount() const;

	[[nodiscard]] PicoCalcKeyboardInputResult handleRawEvent(
	    std::uint8_t status,
	    std::uint8_t scanCode,
	    std::uint32_t now,
	    bool rawEventReporting);
	[[nodiscard]] PicoCalcKeyboardInputResult nextRepeatEvent(std::uint32_t now);
	[[nodiscard]] PicoCalcKeyboardInputResult releaseNextStaleScanCode(std::uint32_t now);

private:
	static constexpr std::uint8_t keyEventPressed = 0x01;
	static constexpr std::uint8_t keyEventHold = 0x02;
	static constexpr std::uint8_t keyEventReleased = 0x03;
	static constexpr std::uint32_t keyRepeatDelayMs = 500;
	static constexpr std::uint32_t keyRepeatIntervalMs = 60;
	static constexpr std::uint32_t staleScanCodeReleaseMs = 2000;

	[[nodiscard]] static std::uint32_t _rawEvent(std::uint8_t status, std::uint8_t scanCode);
	[[nodiscard]] static std::uint64_t _keyMask(lexxos::devices::KeyCode key);
	[[nodiscard]] InputEvent _makeEvent(
	    InputEventType type,
	    lexxos::devices::KeyCode key,
	    std::uint32_t timestampMs,
	    std::uint8_t character,
	    std::uint8_t scanCode) const;
	[[nodiscard]] bool _handleModifierEvent(std::uint8_t status, std::uint8_t scanCode);

	std::uint64_t _pressedKeys{0};
	std::uint8_t _modifierKeys{0};
	PicoCalcScanCodeState _pressedScanCodes{};
	std::array<std::uint32_t, scanCodeCount> _nextRepeatMs{};
	std::array<std::uint32_t, scanCodeCount> _pressedScanCodeMs{};
	std::uint32_t _staleScanCodeReleases{0};
	std::uint32_t _staleReleaseCount{0};
};

} // namespace lexxos::devices::input_detail
