#include <lexxos/devices/input/detail/picocalc_keyboard_runtime_service.h>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/input/detail/picocalc_keyboard_controller.h>

namespace lexxos::devices::input_detail {

PicoCalcKeyboardRuntimeService::PicoCalcKeyboardRuntimeService(PicoCalcKeyboardController& controller)
    : _controller(controller) {
}

std::string_view PicoCalcKeyboardRuntimeService::name() const {
	return _controller.name();
}

bool PicoCalcKeyboardRuntimeService::isReady() const {
	return _controller.isReady();
}

bool PicoCalcKeyboardRuntimeService::southbridgeSnapshot(PicoCalcSouthbridgeSnapshot& snapshot) const {
	if (!_controller._southbridgeService.hasSnapshot()) {
		return false;
	}
	snapshot = _controller._southbridgeService.snapshot();
	return true;
}

std::uint32_t PicoCalcKeyboardRuntimeService::southbridgeSnapshotMs() const {
	return _controller._southbridgeService.lastRefreshMs();
}

std::uint32_t PicoCalcKeyboardRuntimeService::southbridgeFullSnapshotMs() const {
	return _controller._southbridgeService.lastFullRefreshMs();
}

bool PicoCalcKeyboardRuntimeService::southbridgeLastRefreshOk() const {
	return _controller._southbridgeService.lastRefreshOk();
}

std::uint32_t PicoCalcKeyboardRuntimeService::southbridgeSnapshotUpdates() const {
	return _controller._southbridgeService.successfulRefreshes();
}

std::uint32_t PicoCalcKeyboardRuntimeService::southbridgeSnapshotFailures() const {
	return _controller._southbridgeService.failedRefreshes();
}

PicoCalcSouthbridgeCommandStatus PicoCalcKeyboardRuntimeService::southbridgeLastCommandStatus() const {
	return _controller._southbridgeService.lastCommandStatus();
}

bool PicoCalcKeyboardRuntimeService::refreshSouthbridgeSnapshot(std::uint32_t nowMs) {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.refresh(nowMs);
}

bool PicoCalcKeyboardRuntimeService::requestLcdBacklight(std::uint8_t value, std::uint32_t nowMs) {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.requestLcdBacklight(value, nowMs);
}

bool PicoCalcKeyboardRuntimeService::requestKeyboardBacklight(std::uint8_t value, std::uint32_t nowMs) {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.requestKeyboardBacklight(value, nowMs);
}

bool PicoCalcKeyboardRuntimeService::requestSouthbridgeReset(std::uint8_t delaySeconds, std::uint32_t nowMs) {
	if (!_controller._bus.isReady()) {
		return false;
	}
	bool const ok = _controller._southbridgeService.requestReset(delaySeconds, nowMs);
	if (ok) {
		_controller._ready = false;
		_controller._recovery.recordSouthbridgeResetAttempt(nowMs);
		_controller._recovery.recordSouthbridgeResetSuccess(nowMs);
		_controller._diagnostics.recordSouthbridgeResetAttempt();
		_controller._diagnostics.recordSouthbridgeResetSuccess();
		_controller._resetRuntimeState();
	}
	return ok;
}

bool PicoCalcKeyboardRuntimeService::requestPowerOff(std::uint32_t nowMs) {
	if (!_controller._bus.isReady()) {
		return false;
	}
	return _controller._southbridgeService.requestPowerOff(nowMs);
}

#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
bool PicoCalcKeyboardRuntimeService::readJcsFirmwareInfo(PicoCalcJcsFirmwareInfo& info) const {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.readJcsFirmwareInfo(info);
}

bool PicoCalcKeyboardRuntimeService::readJcsRtcConfig(PicoCalcJcsRtcConfig& config) const {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.readJcsRtcConfig(config);
}

bool PicoCalcKeyboardRuntimeService::writeJcsRtcConfig(PicoCalcJcsRtcConfig config, std::uint32_t nowMs) {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.writeJcsRtcConfig(config, nowMs);
}

bool PicoCalcKeyboardRuntimeService::readJcsRtcDateTime(lexxos::devices::rtc::DateTime& value) const {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.readJcsRtcDateTime(value);
}

bool PicoCalcKeyboardRuntimeService::writeJcsRtcDateTime(
    lexxos::devices::rtc::DateTime const& value,
    std::uint32_t nowMs) {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.writeJcsRtcDateTime(value, nowMs);
}

bool PicoCalcKeyboardRuntimeService::readJcsRtcAlarm(lexxos::devices::rtc::DateTime& value) const {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.readJcsRtcAlarm(value);
}

bool PicoCalcKeyboardRuntimeService::writeJcsRtcAlarm(
    lexxos::devices::rtc::DateTime const& value,
    std::uint32_t nowMs) {
	if (!_controller._ready) {
		return false;
	}
	return _controller._southbridgeService.writeJcsRtcAlarm(value, nowMs);
}
#endif

std::uint32_t PicoCalcKeyboardRuntimeService::rawState() const {
	return _controller.rawState();
}

std::uint32_t PicoCalcKeyboardRuntimeService::rawKeyCount() const {
	return _controller.rawKeyCount();
}

std::uint32_t PicoCalcKeyboardRuntimeService::staleScanCodeReleases() const {
	return _controller.staleScanCodeReleases();
}

std::uint32_t PicoCalcKeyboardRuntimeService::staleReleaseCount() const {
	return _controller.staleReleaseCount();
}

} // namespace lexxos::devices::input_detail
