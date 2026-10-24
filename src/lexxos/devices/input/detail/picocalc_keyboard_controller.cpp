#include <lexxos/devices/input/detail/picocalc_keyboard_controller.h>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices::input_detail {

PicoCalcKeyboardController::PicoCalcKeyboardController(
    lexxos::devices::I2cBus& bus,
    lexxos::platform::PlatformServices const& platform,
    std::uint8_t address)
    : _bus(bus)
    , _platformServices(platform)
    , _runtimeService(*this)
    , _diagnostics(platform.console)
    , _southbridge(bus, address)
    , _transport(_southbridge)
    , _southbridgeService(_southbridge) {
}

bool PicoCalcKeyboardController::initialize() {
	_ready = false;
	_recovery.recordInitializeAttempt(_uptimeMs());
	_resetRuntimeState();

	if (!_bus.isReady()) {
		_recordCommunicationFailure("i2c-bus-not-ready", _uptimeMs());
		return false;
	}

	auto const result = _initializer.initialize(_transport, _southbridgeService, _platformServices.clock);
	_ready = result.stateReadOk;
	if (_ready) {
		_diagnostics.recordCommunicationSuccess();
		_lastKeyboardStatus = result.state.state;
		_lastKeyboardCount = result.pendingFifoCount;

		if (!result.drainOk) {
			_handleCommunicationFailure("drain-keyboard-fifo-read");
		}
		_lastKeyboardCount = 0;
	} else {
		auto const now = _uptimeMs();
		_recordCommunicationFailure("initialize-keyboard-state-read", now);
		_tryRecoverSouthbridge(now);
	}
	return _ready;
}

void PicoCalcKeyboardController::update() {
	if (!_ready) {
		auto const now = _uptimeMs();
		if (!_recovery.canAttemptInitialize(now)) {
			return;
		}
		if (!initialize()) {
			return;
		}
	}

	auto const now = _uptimeMs();
	auto const pollResult = _fifoPoller.poll(_transport, _fifoEvents);
	_lastKeyboardCount = pollResult.eventCount;
	if (pollResult.successfulReads != 0) {
		_diagnostics.recordCommunicationSuccess();
		_lastKeyboardStatus = pollResult.lastStatus;
	}
	if (!pollResult.communicationOk) {
		_handleCommunicationFailure("update-keyboard-fifo-read");
		return;
	}

	if (pollResult.eventCount != 0) {
		_lastKeyboardActivityMs = now;
	}

	for (std::uint8_t index = 0; index < pollResult.eventCount; ++index) {
		auto const& event = _fifoEvents[index];
		_eventBridge.handleRawEvent(event.status, event.code, now);
	}

	_eventBridge.updateRepeatEvents(now);
	_eventBridge.releaseStaleScanCodes(now);
	if (_southbridgeService.refreshDue(now) && _canRefreshSouthbridge(now) && !_southbridgeService.refresh(now)) {
		_recordSouthbridgeRefreshFailure(now);
	}
}

void PicoCalcKeyboardController::setRawEventReporting(bool enabled) {
	_eventBridge.setRawEventReporting(enabled);
}

std::string_view PicoCalcKeyboardController::name() const {
	return "picocalc-keyboard";
}

bool PicoCalcKeyboardController::isReady() const {
	return _ready || _bus.isReady();
}

bool PicoCalcKeyboardController::pollEvent(lexxos::devices::InputEvent& event) {
	if (!_eventBridge.hasPendingEvents()) {
		update();
	}
	return _eventBridge.pollEvent(event);
}

bool PicoCalcKeyboardController::isPressed(lexxos::devices::KeyCode key) const {
	return _eventBridge.isPressed(key);
}

PicoCalcSouthbridgeRuntimeService& PicoCalcKeyboardController::runtimeService() {
	return _runtimeService;
}

PicoCalcSouthbridgeRuntimeService const& PicoCalcKeyboardController::runtimeService() const {
	return _runtimeService;
}

std::uint32_t PicoCalcKeyboardController::rawState() const {
	return _eventBridge.rawState();
}

std::uint32_t PicoCalcKeyboardController::rawKeyCount() const {
	return _lastKeyboardCount;
}

std::uint32_t PicoCalcKeyboardController::rawRowCount() const {
	return 0;
}

std::uint32_t PicoCalcKeyboardController::rawColumnCount() const {
	return 0;
}

std::uint32_t PicoCalcKeyboardController::staleScanCodeReleases() const {
	return _eventBridge.staleScanCodeReleases();
}

std::uint32_t PicoCalcKeyboardController::staleReleaseCount() const {
	return _eventBridge.staleReleaseCount();
}

void PicoCalcKeyboardController::_resetRuntimeState() {
	_eventBridge.clear();
	_lastKeyboardActivityMs = 0;
	_lastKeyboardStatus = 0;
	_lastKeyboardCount = 0;

	_southbridgeService.invalidate();
}

void PicoCalcKeyboardController::_handleCommunicationFailure(std::string_view stage) {
	auto const now = _uptimeMs();
	_recordCommunicationFailure(stage, now);
	_ready = false;
	_recovery.recordRuntimeFailure(now);
	_resetRuntimeState();
	_tryRecoverSouthbridge(now);
}

void PicoCalcKeyboardController::_recordCommunicationFailure(std::string_view stage, std::uint32_t now) {
	_diagnostics.recordCommunicationFailure(stage, now, _diagnosticContext());
}

void PicoCalcKeyboardController::_recordSouthbridgeRefreshFailure(std::uint32_t now) {
	_diagnostics.recordSouthbridgeRefreshFailure(now, _diagnosticContext());
}

void PicoCalcKeyboardController::_tryRecoverSouthbridge(std::uint32_t now) {
	if (!_recovery.shouldAttemptSouthbridgeReset(_diagnostics.consecutiveCommunicationFailures(), now)) {
		return;
	}

	_diagnostics.recordSouthbridgeResetAttempt();
	_recovery.recordSouthbridgeResetAttempt(now);
	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	bool const ok = _southbridge.requestReset(0, &state);
	if (ok && state == PicoCalcSouthbridgeCommandState::Ok) {
		_diagnostics.recordSouthbridgeResetSuccess();
		_recovery.recordSouthbridgeResetSuccess(now);
		_diagnostics.writeSouthbridgeResetResult(true, now);
		return;
	}

	_diagnostics.writeSouthbridgeResetResult(false, now);
}

PicoCalcKeyboardDiagnosticContext PicoCalcKeyboardController::_diagnosticContext() const {
	return {
	    .ready = _ready,
	    .lastKeyboardStatus = _lastKeyboardStatus,
	    .lastKeyboardCount = _lastKeyboardCount,
	    .lastRawEvent = _eventBridge.rawState(),
	    .southbridgeSnapshotOk = _southbridgeService.lastRefreshOk(),
	    .southbridgeSnapshotFailures = _southbridgeService.failedRefreshes(),
	};
}

bool PicoCalcKeyboardController::_canRefreshSouthbridge(std::uint32_t now) const {
	if (_eventBridge.hasPendingEvents() || _eventBridge.hasActiveInput()) {
		return false;
	}
	if (_lastKeyboardActivityMs == 0) {
		return true;
	}
	return now - _lastKeyboardActivityMs >= southbridgeRefreshQuietMs;
}

std::uint32_t PicoCalcKeyboardController::_uptimeMs() const {
	auto const& clock = _platformServices.clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

} // namespace lexxos::devices::input_detail
