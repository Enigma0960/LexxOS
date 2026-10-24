#include <lexxos/devices/input/detail/picocalc_keyboard_recovery_policy.h>

namespace lexxos::devices::input_detail {

void PicoCalcKeyboardRecoveryPolicy::recordInitializeAttempt(std::uint32_t now) {
	_lastInitializeAttemptMs = now;
}

void PicoCalcKeyboardRecoveryPolicy::recordRuntimeFailure(std::uint32_t now) {
	_lastInitializeAttemptMs = now;
}

void PicoCalcKeyboardRecoveryPolicy::recordSouthbridgeResetAttempt(std::uint32_t now) {
	_lastSouthbridgeResetAttemptMs = now;
}

void PicoCalcKeyboardRecoveryPolicy::recordSouthbridgeResetSuccess(std::uint32_t now) {
	_lastInitializeAttemptMs = now;
}

bool PicoCalcKeyboardRecoveryPolicy::canAttemptInitialize(std::uint32_t now) const {
	if (_lastSouthbridgeResetAttemptMs != 0 && now - _lastSouthbridgeResetAttemptMs < southbridgeResetRecoveryMs) {
		return false;
	}
	if (_lastInitializeAttemptMs != 0 && now - _lastInitializeAttemptMs < initializeBackoffMs) {
		return false;
	}
	return true;
}

bool PicoCalcKeyboardRecoveryPolicy::shouldAttemptSouthbridgeReset(
    std::uint32_t consecutiveCommunicationFailures,
    std::uint32_t now) const {
	if (consecutiveCommunicationFailures < autoResetFailureThreshold) {
		return false;
	}
	if (_lastSouthbridgeResetAttemptMs != 0 && now - _lastSouthbridgeResetAttemptMs < southbridgeResetBackoffMs) {
		return false;
	}
	return true;
}

} // namespace lexxos::devices::input_detail
