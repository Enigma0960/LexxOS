#pragma once

#include <cstdint>
#include <string_view>

namespace lexxos::platform {
struct PlatformConsoleService;
} // namespace lexxos::platform

namespace lexxos::devices::input_detail {

struct PicoCalcKeyboardDiagnosticContext {
	bool ready{false};
	std::uint8_t lastKeyboardStatus{0};
	std::uint8_t lastKeyboardCount{0};
	std::uint32_t lastRawEvent{0};
	bool southbridgeSnapshotOk{false};
	std::uint32_t southbridgeSnapshotFailures{0};
};

class PicoCalcKeyboardDiagnostics final {
public:
	explicit PicoCalcKeyboardDiagnostics(lexxos::platform::PlatformConsoleService const& console);

	void recordCommunicationSuccess();
	void recordCommunicationFailure(
	    std::string_view stage,
	    std::uint32_t now,
	    PicoCalcKeyboardDiagnosticContext const& context);
	void recordSouthbridgeRefreshFailure(std::uint32_t now, PicoCalcKeyboardDiagnosticContext const& context);
	void recordSouthbridgeResetAttempt();
	void recordSouthbridgeResetSuccess();
	void writeSouthbridgeResetResult(bool success, std::uint32_t now) const;

	[[nodiscard]] std::uint32_t consecutiveCommunicationFailures() const;

private:
	static constexpr std::uint32_t diagnosticLogIntervalMs = 1000;

	[[nodiscard]] bool _diagnosticLogDue(std::uint32_t now) const;
	void _writeFailure(std::string_view stage, std::uint32_t now, PicoCalcKeyboardDiagnosticContext const& context) const;
	void _writeText(std::string_view text) const;
	void _writeUnsigned(std::uint32_t value) const;
	void _writeHex8(std::uint8_t value) const;
	void _writeHex32(std::uint32_t value) const;

	lexxos::platform::PlatformConsoleService const& _console;
	std::uint32_t _communicationFailures{0};
	std::uint32_t _consecutiveCommunicationFailures{0};
	std::uint32_t _southbridgeRefreshFailures{0};
	std::uint32_t _southbridgeResetAttempts{0};
	std::uint32_t _southbridgeResetSuccesses{0};
	std::uint32_t _lastDiagnosticLogMs{0};
};

} // namespace lexxos::devices::input_detail
