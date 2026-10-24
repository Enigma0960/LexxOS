#pragma once

#include <cstdint>

namespace lexxos::devices::input_detail {

class PicoCalcKeyboardRecoveryPolicy final {
public:
	void recordInitializeAttempt(std::uint32_t now);
	void recordRuntimeFailure(std::uint32_t now);
	void recordSouthbridgeResetAttempt(std::uint32_t now);
	void recordSouthbridgeResetSuccess(std::uint32_t now);

	[[nodiscard]] bool canAttemptInitialize(std::uint32_t now) const;
	[[nodiscard]] bool shouldAttemptSouthbridgeReset(std::uint32_t consecutiveCommunicationFailures, std::uint32_t now) const;

private:
	static constexpr std::uint32_t initializeBackoffMs = 250;
	static constexpr std::uint32_t southbridgeResetRecoveryMs = 1500;
	static constexpr std::uint32_t southbridgeResetBackoffMs = 5000;
	// Do not automatically reset the PicoCalc southbridge while the keyboard has
	// never communicated successfully. The southbridge also owns power-button
	// handling, so repeated reset writes during bring-up can make a bus fault
	// harder to diagnose. Manual reset requests still go through the runtime service.
	static constexpr std::uint32_t autoResetFailureThreshold = 0xFFFFFFFFU;

	std::uint32_t _lastInitializeAttemptMs{0};
	std::uint32_t _lastSouthbridgeResetAttemptMs{0};
};

} // namespace lexxos::devices::input_detail
