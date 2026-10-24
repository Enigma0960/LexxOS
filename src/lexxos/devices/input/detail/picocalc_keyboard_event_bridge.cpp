#include <lexxos/devices/input/detail/picocalc_keyboard_event_bridge.h>

namespace lexxos::devices::input_detail {

void PicoCalcKeyboardEventBridge::clear() {
	_inputState.clear();
	_events.clear();
	_lastRawEvent = 0;
}

void PicoCalcKeyboardEventBridge::setRawEventReporting(bool enabled) {
	_rawEventReporting = enabled;
}

bool PicoCalcKeyboardEventBridge::pollEvent(lexxos::devices::InputEvent& event) {
	return _events.pop(event);
}

bool PicoCalcKeyboardEventBridge::hasPendingEvents() const {
	return !_events.empty();
}

bool PicoCalcKeyboardEventBridge::hasActiveInput() const {
	return _inputState.hasActiveInput();
}

bool PicoCalcKeyboardEventBridge::isPressed(lexxos::devices::KeyCode key) const {
	return _inputState.isPressed(key);
}

std::uint32_t PicoCalcKeyboardEventBridge::rawState() const {
	return _lastRawEvent;
}

std::uint32_t PicoCalcKeyboardEventBridge::staleScanCodeReleases() const {
	return _inputState.staleScanCodeReleases();
}

std::uint32_t PicoCalcKeyboardEventBridge::staleReleaseCount() const {
	return _inputState.staleReleaseCount();
}

void PicoCalcKeyboardEventBridge::handleRawEvent(std::uint8_t status, std::uint8_t code, std::uint32_t now) {
	_acceptInputResult(_inputState.handleRawEvent(status, code, now, _rawEventReporting));
}

void PicoCalcKeyboardEventBridge::updateRepeatEvents(std::uint32_t now) {
	while (!_events.full()) {
		auto const result = _inputState.nextRepeatEvent(now);
		if (!result.rawEventAccepted) {
			return;
		}
		_acceptInputResult(result);
	}
}

void PicoCalcKeyboardEventBridge::releaseStaleScanCodes(std::uint32_t now) {
	while (true) {
		auto const result = _inputState.releaseNextStaleScanCode(now);
		if (!result.rawEventAccepted) {
			return;
		}
		_lastRawEvent = result.rawEvent;
		if (result.hasEvent && !_events.full()) {
			_events.push(result.event);
		}
	}
}

void PicoCalcKeyboardEventBridge::_acceptInputResult(PicoCalcKeyboardInputResult const& result) {
	if (result.rawEventAccepted) {
		_lastRawEvent = result.rawEvent;
	}
	if (result.hasEvent) {
		_events.push(result.event);
	}
}

} // namespace lexxos::devices::input_detail
