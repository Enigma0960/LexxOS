#include <lexxos/devices/input/detail/picocalc_scan_code_state.h>

namespace lexxos::devices::input_detail {

void PicoCalcScanCodeState::clear() {
	_pressed.fill(0);
}

bool PicoCalcScanCodeState::anyPressed() const {
	for (auto const byte : _pressed) {
		if (byte != 0) {
			return true;
		}
	}
	return false;
}

bool PicoCalcScanCodeState::isPressed(std::uint8_t code) const {
	auto const byteIndex = static_cast<std::size_t>(code >> 3U);
	auto const bit = static_cast<std::uint8_t>(1U << (code & 0x07U));
	return byteIndex < _pressed.size() && (_pressed[byteIndex] & bit) != 0;
}

void PicoCalcScanCodeState::setPressed(std::uint8_t code, bool pressed) {
	auto const byteIndex = static_cast<std::size_t>(code >> 3U);
	if (byteIndex >= _pressed.size()) {
		return;
	}

	auto const bit = static_cast<std::uint8_t>(1U << (code & 0x07U));
	if (pressed) {
		_pressed[byteIndex] |= bit;
	} else {
		_pressed[byteIndex] &= static_cast<std::uint8_t>(~bit);
	}
}

} // namespace lexxos::devices::input_detail
