#include <lexxos/devices/input/detail/picocalc_keyboard_input_state.h>

#include <lexxos/devices/input/picocalc_keyboard_layout.h>

namespace lexxos::devices::input_detail {

void PicoCalcKeyboardInputState::clear() {
	_pressedKeys = 0;
	_modifierKeys = 0;
	_pressedScanCodes.clear();
	_nextRepeatMs.fill(0);
	_pressedScanCodeMs.fill(0);
}

bool PicoCalcKeyboardInputState::isPressed(lexxos::devices::KeyCode key) const {
	auto const mask = _keyMask(key);
	return mask != 0 && (_pressedKeys & mask) != 0;
}

bool PicoCalcKeyboardInputState::hasActiveInput() const {
	return _pressedKeys != 0 || _modifierKeys != 0 || _pressedScanCodes.anyPressed();
}

std::uint32_t PicoCalcKeyboardInputState::staleScanCodeReleases() const {
	return _staleScanCodeReleases;
}

std::uint32_t PicoCalcKeyboardInputState::staleReleaseCount() const {
	return _staleReleaseCount;
}

PicoCalcKeyboardInputResult PicoCalcKeyboardInputState::handleRawEvent(
    std::uint8_t status,
    std::uint8_t scanCode,
    std::uint32_t now,
    bool rawEventReporting) {
	if (status != keyEventPressed && status != keyEventHold && status != keyEventReleased) {
		return {};
	}

	if (picoCalcKeyboardIsModifierScanCode(scanCode)) {
		if (!_handleModifierEvent(status, scanCode)) {
			return {};
		}
		return {
		    .rawEventAccepted = true,
		    .rawEvent = _rawEvent(status, scanCode),
		};
	}

	if (status == keyEventHold) {
		return {};
	}

	auto const mapping = picoCalcKeyboardMapScanCode(scanCode, _modifierKeys);
	bool const knownEvent = mapping.key != KeyCode::Unknown || mapping.character != 0;
	if (!knownEvent && !rawEventReporting) {
		return {
		    .rawEventAccepted = true,
		    .rawEvent = _rawEvent(status, scanCode),
		};
	}

	auto const mask = _keyMask(mapping.key);
	bool const wasKeyPressed = mask != 0 && (_pressedKeys & mask) != 0;
	bool const wasScanPressed = _pressedScanCodes.isPressed(scanCode);
	bool const wasPressed = wasKeyPressed || wasScanPressed;

	if (status == keyEventPressed) {
		if (wasPressed) {
			return {};
		}
		_pressedScanCodes.setPressed(scanCode, true);
		_pressedScanCodeMs[scanCode] = now;
		if (mask != 0) {
			_pressedKeys |= mask;
		}
		_nextRepeatMs[scanCode] = picoCalcKeyboardIsRepeatable(mapping, scanCode) ? now + keyRepeatDelayMs : 0;
		return {
		    .rawEventAccepted = true,
		    .rawEvent = _rawEvent(status, scanCode),
		    .hasEvent = true,
		    .event = _makeEvent(InputEventType::KeyDown, mapping.key, now, mapping.character, scanCode),
		};
	}

	_nextRepeatMs[scanCode] = 0;
	_pressedScanCodeMs[scanCode] = 0;
	_pressedScanCodes.setPressed(scanCode, false);
	if (mask != 0) {
		_pressedKeys &= ~mask;
	}
	if (!wasPressed) {
		return {};
	}
	return {
	    .rawEventAccepted = true,
	    .rawEvent = _rawEvent(status, scanCode),
	    .hasEvent = true,
	    .event = _makeEvent(InputEventType::KeyUp, mapping.key, now, mapping.character, scanCode),
	};
}

PicoCalcKeyboardInputResult PicoCalcKeyboardInputState::nextRepeatEvent(std::uint32_t now) {
	for (std::size_t code = 0; code < _nextRepeatMs.size(); ++code) {
		auto const dueMs = _nextRepeatMs[code];
		if (dueMs == 0 || now < dueMs) {
			continue;
		}

		auto const scanCode = static_cast<std::uint8_t>(code);
		if (!_pressedScanCodes.isPressed(scanCode)) {
			_nextRepeatMs[code] = 0;
			continue;
		}

		auto const mapping = picoCalcKeyboardMapScanCode(scanCode, _modifierKeys);
		if (!picoCalcKeyboardIsRepeatable(mapping, scanCode)) {
			_nextRepeatMs[code] = 0;
			continue;
		}

		_nextRepeatMs[code] = now + keyRepeatIntervalMs;
		return {
		    .rawEventAccepted = true,
		    .rawEvent = _rawEvent(keyEventPressed, scanCode),
		    .hasEvent = true,
		    .event = _makeEvent(InputEventType::KeyDown, mapping.key, now, mapping.character, scanCode),
		};
	}
	return {};
}

PicoCalcKeyboardInputResult PicoCalcKeyboardInputState::releaseNextStaleScanCode(std::uint32_t now) {
	for (std::size_t code = 0; code < _pressedScanCodeMs.size(); ++code) {
		auto const pressedMs = _pressedScanCodeMs[code];
		if (pressedMs == 0 || now - pressedMs < staleScanCodeReleaseMs) {
			continue;
		}

		auto const scanCode = static_cast<std::uint8_t>(code);
		if (!_pressedScanCodes.isPressed(scanCode)) {
			_pressedScanCodeMs[code] = 0;
			_nextRepeatMs[code] = 0;
			continue;
		}

		auto const mapping = picoCalcKeyboardMapScanCode(scanCode, _modifierKeys);
		if (picoCalcKeyboardIsRepeatable(mapping, scanCode)) {
			continue;
		}

		auto const mask = _keyMask(mapping.key);
		_pressedScanCodeMs[code] = 0;
		_nextRepeatMs[code] = 0;
		_pressedScanCodes.setPressed(scanCode, false);
		if (mask != 0) {
			_pressedKeys &= ~mask;
		}
		++_staleScanCodeReleases;
		++_staleReleaseCount;

		return {
		    .rawEventAccepted = true,
		    .rawEvent = _rawEvent(keyEventReleased, scanCode),
		    .hasEvent = true,
		    .event = _makeEvent(InputEventType::KeyUp, mapping.key, now, mapping.character, scanCode),
		};
	}
	return {};
}

std::uint32_t PicoCalcKeyboardInputState::_rawEvent(std::uint8_t status, std::uint8_t scanCode) {
	return (static_cast<std::uint32_t>(status) << 8U) | scanCode;
}

std::uint64_t PicoCalcKeyboardInputState::_keyMask(lexxos::devices::KeyCode key) {
	auto const index = static_cast<std::uint32_t>(key);
	if (index >= 64) {
		return 0;
	}
	return 1ULL << index;
}

InputEvent PicoCalcKeyboardInputState::_makeEvent(
    InputEventType type,
    lexxos::devices::KeyCode key,
    std::uint32_t timestampMs,
    std::uint8_t character,
    std::uint8_t scanCode) const {
	return InputEvent{
	    .type = type,
	    .key = key,
	    .timestampMs = timestampMs,
	    .character = character,
	    .scanCode = scanCode,
	    .modifiers = _modifierKeys,
	};
}

bool PicoCalcKeyboardInputState::_handleModifierEvent(std::uint8_t status, std::uint8_t scanCode) {
	std::uint8_t const mask = picoCalcKeyboardModifierMask(scanCode);
	if (mask == 0) {
		return false;
	}

	bool const wasPressed = (_modifierKeys & mask) != 0;
	if (status == keyEventPressed && wasPressed) {
		return false;
	}
	if (status == keyEventHold && wasPressed) {
		return false;
	}
	if (status == keyEventReleased && !wasPressed) {
		return false;
	}

	if (status == keyEventReleased) {
		_modifierKeys &= ~mask;
	} else if (status == keyEventPressed || status == keyEventHold) {
		_modifierKeys |= mask;
	}
	return true;
}

} // namespace lexxos::devices::input_detail
