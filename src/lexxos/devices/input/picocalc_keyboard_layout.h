#pragma once

#include <cstdint>

#include <lexxos/devices/input_device.h>

namespace lexxos::devices {

struct PicoCalcKeyboardMapping {
	KeyCode key{KeyCode::Unknown};
	std::uint8_t character{0};
	std::uint8_t modifierMask{0};
};

[[nodiscard]] PicoCalcKeyboardMapping picoCalcKeyboardMapScanCode(std::uint8_t scanCode, std::uint8_t modifiers = 0);
[[nodiscard]] bool picoCalcKeyboardIsModifierScanCode(std::uint8_t scanCode);
[[nodiscard]] std::uint8_t picoCalcKeyboardModifierMask(std::uint8_t scanCode);
[[nodiscard]] bool picoCalcKeyboardIsRepeatable(PicoCalcKeyboardMapping mapping, std::uint8_t scanCode);

} // namespace lexxos::devices
