#include <lexxos/devices/input/detail/picocalc_keyboard_diagnostics.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices::input_detail {
namespace {

	char diagnosticHexDigit(std::uint8_t value) {
		return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
	}

} // namespace

PicoCalcKeyboardDiagnostics::PicoCalcKeyboardDiagnostics(lexxos::platform::PlatformConsoleService const& console)
    : _console(console) {
}

void PicoCalcKeyboardDiagnostics::recordCommunicationSuccess() {
	_consecutiveCommunicationFailures = 0;
}

void PicoCalcKeyboardDiagnostics::recordCommunicationFailure(
    std::string_view stage,
    std::uint32_t now,
    PicoCalcKeyboardDiagnosticContext const& context) {
	++_communicationFailures;
	++_consecutiveCommunicationFailures;

	if (_diagnosticLogDue(now)) {
		_writeFailure(stage, now, context);
		_lastDiagnosticLogMs = now;
	}
}

void PicoCalcKeyboardDiagnostics::recordSouthbridgeRefreshFailure(
    std::uint32_t now,
    PicoCalcKeyboardDiagnosticContext const& context) {
	++_southbridgeRefreshFailures;

	if (_diagnosticLogDue(now)) {
		_writeFailure("southbridge-refresh", now, context);
		_lastDiagnosticLogMs = now;
	}
}

void PicoCalcKeyboardDiagnostics::recordSouthbridgeResetAttempt() {
	++_southbridgeResetAttempts;
}

void PicoCalcKeyboardDiagnostics::recordSouthbridgeResetSuccess() {
	++_southbridgeResetSuccesses;
}

void PicoCalcKeyboardDiagnostics::writeSouthbridgeResetResult(bool success, std::uint32_t now) const {
	_writeText(success ? "[picocalc-keyboard] southbridge reset requested: uptime-ms="
	                   : "[picocalc-keyboard] southbridge reset failed: uptime-ms=");
	_writeUnsigned(now);
	_writeText(" attempts=");
	_writeUnsigned(_southbridgeResetAttempts);
	_writeText(" successes=");
	_writeUnsigned(_southbridgeResetSuccesses);
	_writeText("\r\n");
}

std::uint32_t PicoCalcKeyboardDiagnostics::consecutiveCommunicationFailures() const {
	return _consecutiveCommunicationFailures;
}

bool PicoCalcKeyboardDiagnostics::_diagnosticLogDue(std::uint32_t now) const {
	return _lastDiagnosticLogMs == 0 || now - _lastDiagnosticLogMs >= diagnosticLogIntervalMs;
}

void PicoCalcKeyboardDiagnostics::_writeFailure(
    std::string_view stage,
    std::uint32_t now,
    PicoCalcKeyboardDiagnosticContext const& context) const {
	if (_console.write == nullptr) {
		return;
	}

	_writeText("[picocalc-keyboard] communication failure: stage=");
	_writeText(stage);
	_writeText(" uptime-ms=");
	_writeUnsigned(now);
	_writeText(" ready=");
	_writeText(context.ready ? "yes" : "no");
	_writeText(" comm-failures=");
	_writeUnsigned(_communicationFailures);
	_writeText(" consecutive=");
	_writeUnsigned(_consecutiveCommunicationFailures);
	_writeText(" sb-refresh-failures=");
	_writeUnsigned(_southbridgeRefreshFailures);
	_writeText(" sb-reset-attempts=");
	_writeUnsigned(_southbridgeResetAttempts);
	_writeText(" sb-reset-successes=");
	_writeUnsigned(_southbridgeResetSuccesses);
	_writeText(" last-state=");
	_writeHex8(context.lastKeyboardStatus);
	_writeText(" fifo=");
	_writeUnsigned(context.lastKeyboardCount);
	_writeText(" raw=");
	_writeHex32(context.lastRawEvent);
	_writeText(" snapshot-ok=");
	_writeText(context.southbridgeSnapshotOk ? "yes" : "no");
	_writeText(" snapshot-failures=");
	_writeUnsigned(context.southbridgeSnapshotFailures);
	_writeText("\r\n");
}

void PicoCalcKeyboardDiagnostics::_writeText(std::string_view text) const {
	if (_console.write != nullptr) {
		_console.write(text);
	}
}

void PicoCalcKeyboardDiagnostics::_writeUnsigned(std::uint32_t value) const {
	char buffer[10]{};
	std::size_t position = sizeof(buffer);
	do {
		buffer[--position] = static_cast<char>('0' + (value % 10U));
		value /= 10U;
	} while (value != 0 && position != 0);
	_writeText({buffer + position, sizeof(buffer) - position});
}

void PicoCalcKeyboardDiagnostics::_writeHex8(std::uint8_t value) const {
	char buffer[4]{
	    '0',
	    'x',
	    diagnosticHexDigit(static_cast<std::uint8_t>((value >> 4U) & 0x0F)),
	    diagnosticHexDigit(static_cast<std::uint8_t>(value & 0x0F)),
	};
	_writeText({buffer, sizeof(buffer)});
}

void PicoCalcKeyboardDiagnostics::_writeHex32(std::uint32_t value) const {
	char buffer[10]{'0', 'x'};
	for (std::uint8_t index = 0; index < 8; ++index) {
		auto const shift = static_cast<std::uint8_t>((7U - index) * 4U);
		buffer[index + 2U] = diagnosticHexDigit(static_cast<std::uint8_t>((value >> shift) & 0x0F));
	}
	_writeText({buffer, sizeof(buffer)});
}

} // namespace lexxos::devices::input_detail
