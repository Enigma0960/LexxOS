#include <lexxos/devices/picocalc/picocalc_southbridge_service.h>

#include <lexxos/devices/picocalc/detail/picocalc_southbridge_requests.h>

namespace lexxos::devices {

PicoCalcSouthbridgeService::PicoCalcSouthbridgeService(PicoCalcSouthbridge& southbridge)
    : _southbridge(southbridge) {
}

bool PicoCalcSouthbridgeService::refresh(std::uint32_t nowMs) {
	return _snapshots.refresh(_southbridge, nowMs);
}

bool PicoCalcSouthbridgeService::refreshDue(std::uint32_t nowMs) const {
	return _snapshots.refreshDue(nowMs);
}

bool PicoCalcSouthbridgeService::refreshIfDue(std::uint32_t nowMs) {
	return _snapshots.refreshIfDue(_southbridge, nowMs);
}

void PicoCalcSouthbridgeService::publishKeyboardState(PicoCalcKeyboardState const& state, std::uint32_t nowMs) {
	_snapshots.publishKeyboardState(state, nowMs);
}

void PicoCalcSouthbridgeService::publishKeyboardState(std::uint8_t state, std::uint8_t value, std::uint32_t nowMs) {
	_snapshots.publishKeyboardState(state, value, nowMs);
}

void PicoCalcSouthbridgeService::invalidate() {
	_snapshots.invalidate();
	_commands.reset();
}

bool PicoCalcSouthbridgeService::requestLcdBacklight(std::uint8_t value, std::uint32_t nowMs) {
	return detail::PicoCalcSouthbridgeRequests::requestLcdBacklight(_southbridge, _snapshots, _commands, value, nowMs);
}

bool PicoCalcSouthbridgeService::requestKeyboardBacklight(std::uint8_t value, std::uint32_t nowMs) {
	return detail::PicoCalcSouthbridgeRequests::requestKeyboardBacklight(_southbridge, _snapshots, _commands, value, nowMs);
}

bool PicoCalcSouthbridgeService::requestReset(std::uint8_t delaySeconds, std::uint32_t nowMs) {
	return detail::PicoCalcSouthbridgeRequests::requestReset(_southbridge, _commands, delaySeconds, nowMs);
}

bool PicoCalcSouthbridgeService::requestPowerOff(std::uint32_t nowMs) {
	return detail::PicoCalcSouthbridgeRequests::requestPowerOff(_southbridge, _commands, nowMs);
}

#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
bool PicoCalcSouthbridgeService::readJcsFirmwareInfo(PicoCalcJcsFirmwareInfo& info) const {
	return _southbridge.readJcsFirmwareInfo(info);
}

bool PicoCalcSouthbridgeService::readJcsRtcConfig(PicoCalcJcsRtcConfig& config) const {
	return _southbridge.readJcsRtcConfig(config);
}

bool PicoCalcSouthbridgeService::writeJcsRtcConfig(PicoCalcJcsRtcConfig config, std::uint32_t nowMs) {
	PicoCalcJcsRtcConfig acceptedConfig{};
	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	bool const ok = _southbridge.setJcsRtcConfig(config, &acceptedConfig, &state);
	if (ok) {
		_snapshots.publishJcsRtcConfig(acceptedConfig, nowMs);
	}
	return ok;
}

bool PicoCalcSouthbridgeService::readJcsRtcDateTime(lexxos::devices::rtc::DateTime& value) const {
	return _southbridge.readJcsRtcDateTime(value);
}

bool PicoCalcSouthbridgeService::writeJcsRtcDateTime(
    lexxos::devices::rtc::DateTime const& value,
    std::uint32_t nowMs) {
	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	bool const ok = _southbridge.setJcsRtcDateTime(value, &state);
	if (ok) {
		_snapshots.publishJcsRtcDateTime(value, nowMs);
	}
	return ok;
}

bool PicoCalcSouthbridgeService::readJcsRtcAlarm(lexxos::devices::rtc::DateTime& value) const {
	return _southbridge.readJcsRtcAlarm(value);
}

bool PicoCalcSouthbridgeService::writeJcsRtcAlarm(
    lexxos::devices::rtc::DateTime const& value,
    std::uint32_t nowMs) {
	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	bool const ok = _southbridge.setJcsRtcAlarm(value, &state);
	if (ok) {
		_snapshots.publishJcsRtcAlarm(value, nowMs);
	}
	return ok;
}
#endif

void PicoCalcSouthbridgeService::setRefreshIntervalMs(std::uint32_t intervalMs) {
	_snapshots.setRefreshIntervalMs(intervalMs);
}

std::uint32_t PicoCalcSouthbridgeService::refreshIntervalMs() const {
	return _snapshots.refreshIntervalMs();
}

bool PicoCalcSouthbridgeService::hasSnapshot() const {
	return _snapshots.hasSnapshot();
}

PicoCalcSouthbridgeSnapshot const& PicoCalcSouthbridgeService::snapshot() const {
	return _snapshots.snapshot();
}

bool PicoCalcSouthbridgeService::lastRefreshOk() const {
	return _snapshots.lastRefreshOk();
}

std::uint32_t PicoCalcSouthbridgeService::lastRefreshMs() const {
	return _snapshots.lastRefreshMs();
}

std::uint32_t PicoCalcSouthbridgeService::lastFullRefreshMs() const {
	return _snapshots.lastFullRefreshMs();
}

std::uint32_t PicoCalcSouthbridgeService::successfulRefreshes() const {
	return _snapshots.successfulRefreshes();
}

std::uint32_t PicoCalcSouthbridgeService::failedRefreshes() const {
	return _snapshots.failedRefreshes();
}

PicoCalcSouthbridgeCommandStatus PicoCalcSouthbridgeService::lastCommandStatus() const {
	return _commands.last();
}

} // namespace lexxos::devices
