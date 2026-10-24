#include <lexxos/devices/picocalc/detail/picocalc_southbridge_command_tracker.h>

namespace lexxos::devices::detail {

void PicoCalcSouthbridgeCommandTracker::reset() {
	_last = {};
}

void PicoCalcSouthbridgeCommandTracker::start(
    PicoCalcSouthbridgeCommandKind kind,
    std::uint8_t requestedValue,
    std::uint32_t nowMs) {
	_last.kind = kind;
	_last.state = PicoCalcSouthbridgeCommandState::Running;
	_last.requestedValue = requestedValue;
	_last.acceptedValue = 0;
	_last.acceptedValueValid = false;
	_last.requestedMs = nowMs;
	_last.completedMs = 0;
}

void PicoCalcSouthbridgeCommandTracker::finish(PicoCalcSouthbridgeCommandState state, std::uint32_t nowMs) {
	_last.state = state;
	_last.completedMs = nowMs;
	_last.acceptedValueValid = false;
}

void PicoCalcSouthbridgeCommandTracker::finish(
    PicoCalcSouthbridgeCommandState state,
    std::uint8_t acceptedValue,
    std::uint32_t nowMs) {
	_last.state = state;
	_last.acceptedValue = acceptedValue;
	_last.acceptedValueValid = state == PicoCalcSouthbridgeCommandState::Ok;
	_last.completedMs = nowMs;
}

PicoCalcSouthbridgeCommandStatus const& PicoCalcSouthbridgeCommandTracker::last() const {
	return _last;
}

} // namespace lexxos::devices::detail
