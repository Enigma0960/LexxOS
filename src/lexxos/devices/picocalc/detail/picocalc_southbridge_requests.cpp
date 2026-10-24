#include <lexxos/devices/picocalc/detail/picocalc_southbridge_requests.h>

#include <lexxos/devices/picocalc/detail/picocalc_southbridge_backlight_mapper.h>

namespace lexxos::devices::detail {

bool PicoCalcSouthbridgeRequests::requestLcdBacklight(
    PicoCalcSouthbridge const& southbridge,
    PicoCalcSouthbridgeSnapshotCache& snapshots,
    PicoCalcSouthbridgeCommandTracker& commands,
    std::uint8_t value,
    std::uint32_t nowMs) {
	commands.start(PicoCalcSouthbridgeCommandKind::LcdBacklight, value, nowMs);
	std::uint8_t const requestedValue = PicoCalcSouthbridgeBacklightMapper::normalizeLcd(value);
	std::uint8_t acceptedValue = 0;
	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	bool const ok = southbridge.setLcdBacklight(requestedValue, &acceptedValue, &state);
	if (ok) {
		snapshots.publishLcdBacklight(acceptedValue, nowMs);
	}
	commands.finish(state, acceptedValue, nowMs);
	return ok;
}

bool PicoCalcSouthbridgeRequests::requestKeyboardBacklight(
    PicoCalcSouthbridge const& southbridge,
    PicoCalcSouthbridgeSnapshotCache& snapshots,
    PicoCalcSouthbridgeCommandTracker& commands,
    std::uint8_t value,
    std::uint32_t nowMs) {
	commands.start(PicoCalcSouthbridgeCommandKind::KeyboardBacklight, value, nowMs);
	std::uint8_t const requestedValue = PicoCalcSouthbridgeBacklightMapper::normalizeKeyboard(value);
	std::uint8_t acceptedValue = 0;
	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	bool const ok = southbridge.setKeyboardBacklight(requestedValue, &acceptedValue, &state);
	if (ok) {
		snapshots.publishKeyboardBacklight(acceptedValue, nowMs);
	}
	commands.finish(state, acceptedValue, nowMs);
	return ok;
}

bool PicoCalcSouthbridgeRequests::requestReset(
    PicoCalcSouthbridge const& southbridge,
    PicoCalcSouthbridgeCommandTracker& commands,
    std::uint8_t delaySeconds,
    std::uint32_t nowMs) {
	commands.start(PicoCalcSouthbridgeCommandKind::Reset, delaySeconds, nowMs);
	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	bool const ok = southbridge.requestReset(delaySeconds, &state);
	commands.finish(state, nowMs);
	return ok;
}

bool PicoCalcSouthbridgeRequests::requestPowerOff(
    PicoCalcSouthbridge const& southbridge,
    PicoCalcSouthbridgeCommandTracker& commands,
    std::uint32_t nowMs) {
	commands.start(PicoCalcSouthbridgeCommandKind::PowerOff, 1, nowMs);
	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	bool const ok = southbridge.requestPowerOff(&state);
	commands.finish(state, nowMs);
	return ok;
}

} // namespace lexxos::devices::detail
