#include <lexxos/devices/input/detail/picocalc_keyboard_diagnostics.h>

#include <algorithm>
#include <cstdio>

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

	char line[640]{};
	auto const& bus = context.i2cFailure;
	auto const length = std::snprintf(line, sizeof(line),
	    "[picocalc-keyboard] communication failure: stage=%.*s uptime-ms=%lu ready=%s comm-failures=%lu consecutive=%lu sb-refresh-failures=%lu sb-reset-attempts=%lu sb-reset-successes=%lu last-state=0x%02X fifo=%u raw=0x%08lX snapshot-ok=%s snapshot-failures=%lu i2c-failure=%s event=0x%lX pending=0x%lX recovery=%lu/%lu\r\n",
	    static_cast<int>(stage.size()), stage.data(), static_cast<unsigned long>(now), context.ready ? "yes" : "no",
	    static_cast<unsigned long>(_communicationFailures), static_cast<unsigned long>(_consecutiveCommunicationFailures),
	    static_cast<unsigned long>(_southbridgeRefreshFailures), static_cast<unsigned long>(_southbridgeResetAttempts),
	    static_cast<unsigned long>(_southbridgeResetSuccesses), static_cast<unsigned>(context.lastKeyboardStatus),
	    static_cast<unsigned>(context.lastKeyboardCount), static_cast<unsigned long>(context.lastRawEvent),
	    context.southbridgeSnapshotOk ? "yes" : "no", static_cast<unsigned long>(context.southbridgeSnapshotFailures),
	    !bus.available ? "unavailable" : bus.nack ? "nack"
	                                              : "timeout",
	    static_cast<unsigned long>(bus.event), static_cast<unsigned long>(bus.pending),
	    static_cast<unsigned long>(bus.recoverySuccesses), static_cast<unsigned long>(bus.recoveryAttempts));

	if (length > 0) {
		_console.write({line, std::min<std::size_t>(length, sizeof(line) - 1)});
	}
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
