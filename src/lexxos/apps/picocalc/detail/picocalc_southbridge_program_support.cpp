#include <commands/command_selection.h>

#include <lexxos/apps/picocalc/detail/picocalc_southbridge_program_support.h>

#include <cstdint>
#include <string_view>

#include <lexxos/apps/detail/i2c_program_support.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/picocalc/picocalc_southbridge.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>

namespace lexxos::apps::i2c_programs::picocalc_southbridge_detail {
namespace {

	char hexDigit(std::uint8_t value) {
		return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
	}

	void writeHex16(lexxos::os::AppContext& context, std::uint16_t value) {
		char buffer[7]{
		    '0',
		    'x',
		    hexDigit(static_cast<std::uint8_t>((value >> 12) & 0x0F)),
		    hexDigit(static_cast<std::uint8_t>((value >> 8) & 0x0F)),
		    hexDigit(static_cast<std::uint8_t>((value >> 4) & 0x0F)),
		    hexDigit(static_cast<std::uint8_t>(value & 0x0F)),
		    '\0',
		};
		context.write(buffer);
	}

	bool parseByteArgument(lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view name,
	    std::uint8_t& output) {
		std::uint32_t value = 0;
		if (!lexxos::apps::i2c_programs::detail::parseUnsigned(arguments.value(name), value) || value > 255) {
			context.writeError("picocalc-sb: invalid --");
			context.writeError(name);
			context.writeErrorLine(" value, expected 0..255");
			return false;
		}

		output = static_cast<std::uint8_t>(value);
		return true;
	}

	bool parseCountArgument(lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view name,
	    std::uint32_t fallback,
	    std::uint32_t maximum,
	    std::uint32_t& output) {
		if (!lexxos::apps::i2c_programs::detail::parseUnsigned(arguments.valueOr(name, ""), output)) {
			output = fallback;
		}

		if (output > maximum) {
			context.writeError("picocalc-sb: --");
			context.writeError(name);
			context.writeError(" is too large, maximum is ");
			lexxos::apps::i2c_programs::detail::writeUnsigned(context, maximum);
			context.writeLine("");
			return false;
		}

		return true;
	}

	void writeReadFailure(lexxos::os::AppContext& context, std::string_view name) {
		context.write(name);
		context.writeLine(": read failed");
	}

	void writeByteReport(lexxos::os::AppContext& context, std::string_view name, std::uint8_t value) {
		context.write(name);
		context.write(": ");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, value);
		context.write(" (");
		lexxos::apps::i2c_programs::detail::writeHex8(context, value);
		context.writeLine(")");
	}

	void writeSnapshotByte(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot,
	    std::string_view name,
	    std::uint16_t validFlag,
	    std::uint8_t value) {
		if (!snapshot.has(validFlag)) {
			context.write(name);
			context.writeLine(": read failed");
			return;
		}

		writeByteReport(context, name, value);
	}

	void writeKeyboardStateReport(lexxos::os::AppContext& context, lexxos::devices::PicoCalcKeyboardState const& state) {
		writeHex16(context, state.raw());
		context.write(" state=");
		lexxos::apps::i2c_programs::detail::writeHex8(context, state.state);
		context.write(" value=");
		lexxos::apps::i2c_programs::detail::writeHex8(context, state.value);
		context.write(" fifo=");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, static_cast<std::uint32_t>(state.fifoCount()));
		context.write(" legacy-fifo=");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, static_cast<std::uint32_t>(state.legacyFifoCount()));
		context.write(" caps=");
		context.write(state.capsLock() ? "on" : "off");
		context.write(" num=");
		context.write(state.numLock() ? "on" : "off");
		context.writeLine("");
	}

	void writeSnapshotKeyboardState(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot) {
		context.write("keyboard-state: ");
		if (snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validKeyboardState)) {
			writeKeyboardStateReport(context, snapshot.keyboardState);
		} else {
			context.writeLine("read failed");
		}
	}

	void writeSnapshotBattery(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot) {
		if (!snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validBattery)) {
			context.writeLine("battery: read failed");
			return;
		}

		context.write("battery: raw=");
		lexxos::apps::i2c_programs::detail::writeHex8(context, snapshot.battery.raw);
		context.write(" charging=");
		context.write(snapshot.battery.charging() ? "yes" : "no");
		context.write(" value=");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, static_cast<std::uint32_t>(snapshot.battery.value()));
		context.writeLine("");
	}

	void writeSnapshotPowerSupport(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot) {
		if (!snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validPowerOffSupport)) {
			context.writeLine("power-off-supported: read failed");
			return;
		}

		context.write("power-off-supported: ");
		context.write(snapshot.powerOffSupport > 0 ? "yes" : "no");
		context.write(" (");
		lexxos::apps::i2c_programs::detail::writeHex8(context, snapshot.powerOffSupport);
		context.writeLine(")");
	}

	void writeSnapshotReport(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot) {
		using Snapshot = lexxos::devices::PicoCalcSouthbridgeSnapshot;
		writeSnapshotByte(context, snapshot, "version", Snapshot::validVersion, snapshot.version);
		writeSnapshotByte(context, snapshot, "config", Snapshot::validConfig, snapshot.config);
		writeSnapshotByte(context, snapshot, "interrupt-status", Snapshot::validInterruptStatus, snapshot.interruptStatus);
		writeSnapshotByte(context, snapshot, "debounce", Snapshot::validDebounce, snapshot.debounce);
		writeSnapshotByte(context, snapshot, "polling-frequency", Snapshot::validPollingFrequency, snapshot.pollingFrequency);
		writeSnapshotKeyboardState(context, snapshot);
		writeSnapshotBattery(context, snapshot);
		writeSnapshotByte(context, snapshot, "lcd-backlight", Snapshot::validLcdBacklight, snapshot.lcdBacklight);
		writeSnapshotByte(context, snapshot, "keyboard-backlight", Snapshot::validKeyboardBacklight, snapshot.keyboardBacklight);
		writeSnapshotByte(context, snapshot, "c64-matrix", Snapshot::validC64Matrix, snapshot.c64Matrix);
		writeSnapshotByte(context, snapshot, "c64-joystick", Snapshot::validC64Joystick, snapshot.c64Joystick);
		writeSnapshotPowerSupport(context, snapshot);
	}

	bool readAndWriteKeyboardState(lexxos::os::AppContext& context, lexxos::devices::PicoCalcSouthbridge const& southbridge) {
		lexxos::devices::PicoCalcKeyboardState state{};
		if (!southbridge.readKeyboardState(state)) {
			writeReadFailure(context, "keyboard-state");
			return false;
		}
		context.write("keyboard-state: ");
		writeKeyboardStateReport(context, state);
		return true;
	}

	bool readAndWriteBattery(lexxos::os::AppContext& context, lexxos::devices::PicoCalcSouthbridge const& southbridge) {
		lexxos::devices::PicoCalcBatteryStatus status{};
		if (!southbridge.readBattery(status)) {
			writeReadFailure(context, "battery");
			return false;
		}

		context.write("battery: raw=");
		lexxos::apps::i2c_programs::detail::writeHex8(context, status.raw);
		context.write(" charging=");
		context.write(status.charging() ? "yes" : "no");
		context.write(" value=");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, static_cast<std::uint32_t>(status.value()));
		context.writeLine("");
		return true;
	}

	bool readAndWriteBacklight(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridge const& southbridge,
	    bool keyboard,
	    std::string_view name) {
		std::uint8_t value = 0;
		bool const ok = keyboard ? southbridge.readKeyboardBacklight(value) : southbridge.readLcdBacklight(value);
		if (!ok) {
			writeReadFailure(context, name);
			return false;
		}

		writeByteReport(context, name, value);
		return true;
	}

	bool readAndWritePowerSupport(lexxos::os::AppContext& context, lexxos::devices::PicoCalcSouthbridge const& southbridge) {
		std::uint8_t value = 0;
		if (!southbridge.readPowerOffSupport(value)) {
			writeReadFailure(context, "power-off-supported");
			return false;
		}

		context.write("power-off-supported: ");
		context.write(value > 0 ? "yes" : "no");
		context.write(" (");
		lexxos::apps::i2c_programs::detail::writeHex8(context, value);
		context.writeLine(")");
		return true;
	}

	lexxos::devices::PicoCalcSouthbridgeRuntimeService* picoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::service(context.devices());
	}

	lexxos::devices::DeviceResourceLock lockPicoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(
		    context.devices(),
		    "picocalc-sb",
		    lexxos::devices::DeviceResourcePriority::Interactive);
	}

	void writeKeyboardOwnedSouthbridgeNotice(lexxos::os::AppContext& context) {
		context.writeLine("picocalc-sb: direct I2C access is skipped while picocalc-keyboard owns the southbridge");
		context.writeLine("picocalc-sb: direct register access can break keyboard state after shell Enter");
		context.writeLine("picocalc-sb: use keyboard-live/input-test for keyboard checks; reboot if input is already stuck");
	}

	bool hasDirectWriteRequest(lexxos::program::ParsedArguments const& arguments) {
		return arguments.has("lcd") || arguments.has("kbd") || arguments.has("reset");
	}

	void writeKeyboardOwnedWriteRefused(lexxos::os::AppContext& context) {
		context.writeLine("picocalc-sb: direct write request refused while picocalc-keyboard is active");
		context.writeLine("picocalc-sb: use backlight/poweroff commands for safe service-backed requests");
	}

	void writeCachedSnapshotReport(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService const& service,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot) {
		context.writeLine("PicoCalc southbridge");
		context.writeLine("source: picocalc-southbridge service cached snapshot");
		context.writeLine("direct-i2c: no");
		context.write("keyboard-ready: ");
		context.writeLine(service.isReady() ? "yes" : "no");

		context.write("snapshot-ms: ");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, service.southbridgeSnapshotMs());
		context.writeLine("");

		context.write("snapshot-full-refresh-ms: ");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, service.southbridgeFullSnapshotMs());
		context.writeLine("");

		context.write("snapshot-last-refresh-ok: ");
		context.writeLine(service.southbridgeLastRefreshOk() ? "yes" : "no");

		context.write("snapshot-fields: ");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, snapshot.validFieldCount());
		context.writeLine("");

		context.write("snapshot-updates: ");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, service.southbridgeSnapshotUpdates());
		context.writeLine("");

		context.write("snapshot-failures: ");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, service.southbridgeSnapshotFailures());
		context.writeLine("");

		context.write("stale-event-releases: ");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, service.staleReleaseCount());
		context.writeLine("");

		context.write("stale-scan-code-releases: ");
		lexxos::apps::i2c_programs::detail::writeUnsigned(context, service.staleScanCodeReleases());
		context.writeLine("");

		writeSnapshotReport(context, snapshot);
	}

} // namespace

bool parseCommandOptions(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    CommandOptions& options) {
	options.busName = arguments.valueOr("bus", "debug");

	std::uint32_t addressValue = lexxos::devices::PicoCalcSouthbridge::defaultAddress;
	if (!lexxos::apps::i2c_programs::detail::parseUnsigned(arguments.valueOr("address", "0x1F"), addressValue) || addressValue > 0x7FU) {
		context.writeErrorLine("picocalc-sb: invalid --address value, expected 7-bit I2C address");
		return false;
	}
	options.address = static_cast<std::uint8_t>(addressValue);

	if (!parseCountArgument(context, arguments, "poll", 0, 64, options.pollCount)) {
		return false;
	}

	std::uint32_t pollDelayMs = 50;
	if (!parseCountArgument(context, arguments, "delay", 50, 5000, pollDelayMs)) {
		return false;
	}
	(void) pollDelayMs;

	options.directRequested = arguments.has("direct-i2c");
	return true;
}

bool hasSouthbridgeService(lexxos::os::AppContext& context) {
	return picoCalcSouthbridge(context) != nullptr;
}

void writeDirectI2cRequired(lexxos::os::AppContext& context) {
	context.writeErrorLine("picocalc-sb: no registered picocalc-southbridge service is available");
	context.writeErrorLine("picocalc-sb: direct I2C diagnostics require explicit --direct-i2c");
	context.writeErrorLine("picocalc-sb: use --direct-i2c only during bring-up without active picocalc-keyboard");
}

int runServiceBacked(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    CommandOptions const& options) {
	auto* service = picoCalcSouthbridge(context);
	if (service == nullptr) {
		writeDirectI2cRequired(context);
		return 1;
	}

	if (options.directRequested) {
		context.writeLine("picocalc-sb: direct I2C diagnostics refused while picocalc-keyboard is active");
		context.writeLine("picocalc-sb: direct register access can break keyboard state after shell Enter");
	}

	if (hasDirectWriteRequest(arguments)) {
		writeKeyboardOwnedWriteRefused(context);
		writeKeyboardOwnedSouthbridgeNotice(context);
		return 1;
	}

	auto serviceLock = lockPicoCalcSouthbridge(context);
	if (!serviceLock) {
		context.writeErrorLine("picocalc-sb: southbridge service is busy");
		return 1;
	}

	lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
	if (service->southbridgeSnapshot(snapshot)) {
		writeCachedSnapshotReport(context, *service, snapshot);
	} else {
		writeKeyboardOwnedSouthbridgeNotice(context);
		context.writeLine("picocalc-sb: no cached keyboard-state snapshot is available yet");
	}

	if (options.pollCount > 0) {
		context.writeLine("keyboard-fifo: direct polling skipped; FIFO is owned by picocalc-keyboard input driver");
		context.writeLine("keyboard-fifo: use keyboard-live/input-test for non-destructive keyboard diagnostics");
	}
	return 0;
}

#if LEXXOS_COMMAND_TEST_OPERATIONS
int runDirectI2c(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    CommandOptions const& options) {
	auto* bus = context.devices().i2cBus(options.busName);
	if (bus == nullptr) {
		context.writeError("picocalc-sb: unknown I2C bus: ");
		context.writeErrorLine(options.busName);
		context.writeErrorLine("available buses:");
		lexxos::apps::i2c_programs::detail::writeI2cBusList(context, true);
		return 1;
	}

	auto busLock = context.devices().lockI2cBus(
	    options.busName,
	    lexxos::devices::DeviceResourcePriority::Interactive,
	    "picocalc-sb");
	if (!busLock) {
		context.writeError("picocalc-sb: I2C bus is busy: ");
		context.writeErrorLine(options.busName);
		return 1;
	}

	lexxos::devices::PicoCalcSouthbridge southbridge(*bus, options.address);
	if (!southbridge.isReady()) {
		context.writeError("picocalc-sb: I2C bus is not ready: ");
		context.writeErrorLine(options.busName);
		return 1;
	}

	context.writeLine("PicoCalc southbridge");
	context.writeLine("source: direct-i2c diagnostic");
	context.writeLine("direct-i2c: yes");
	context.write("bus: ");
	context.writeLine(options.busName);
	context.write("address: ");
	lexxos::apps::i2c_programs::detail::writeHex8(context, southbridge.address());
	context.writeLine("");
	context.write("probe: ");
	context.writeLine(southbridge.probe() ? "yes" : "no");

	std::uint32_t successfulReads = 0;

	if (arguments.has("lcd")) {
		std::uint8_t value = 0;
		if (!parseByteArgument(context, arguments, "lcd", value)) {
			return 2;
		}

		std::uint8_t acceptedValue = 0;
		if (southbridge.setLcdBacklight(value, &acceptedValue)) {
			context.write("lcd-backlight-set: ");
			lexxos::apps::i2c_programs::detail::writeUnsigned(context, acceptedValue);
			context.writeLine("");
			++successfulReads;
		} else {
			context.writeLine("lcd-backlight-set: write failed");
		}
	}

	if (arguments.has("kbd")) {
		std::uint8_t value = 0;
		if (!parseByteArgument(context, arguments, "kbd", value)) {
			return 2;
		}

		std::uint8_t acceptedValue = 0;
		if (southbridge.setKeyboardBacklight(value, &acceptedValue)) {
			context.write("keyboard-backlight-set: ");
			lexxos::apps::i2c_programs::detail::writeUnsigned(context, acceptedValue);
			context.writeLine("");
			++successfulReads;
		} else {
			context.writeLine("keyboard-backlight-set: write failed");
		}
	}

	if (arguments.has("reset")) {
		std::uint8_t value = 0;
		if (!parseByteArgument(context, arguments, "reset", value)) {
			return 2;
		}

		if (southbridge.requestReset(value)) {
			context.write("reset-request: accepted delay=");
			lexxos::apps::i2c_programs::detail::writeUnsigned(context, value);
			context.writeLine("s");
			++successfulReads;
		} else {
			context.writeLine("reset-request: write failed");
		}
	}

	if (readAndWriteKeyboardState(context, southbridge)) {
		++successfulReads;
	}
	if (readAndWriteBattery(context, southbridge)) {
		++successfulReads;
	}
	if (readAndWriteBacklight(context, southbridge, false, "lcd-backlight")) {
		++successfulReads;
	}
	if (readAndWriteBacklight(context, southbridge, true, "keyboard-backlight")) {
		++successfulReads;
	}
	if (readAndWritePowerSupport(context, southbridge)) {
		++successfulReads;
	}

	if (options.pollCount > 0) {
		context.writeLine("keyboard-fifo: skipped; FIFO is owned by picocalc-keyboard input driver");
		context.writeLine("keyboard-fifo: use keyboard-live/input-test for non-destructive keyboard diagnostics");
	}

	if (successfulReads == 0) {
		context.writeErrorLine("picocalc-sb: southbridge did not respond to register reads");
		return 1;
	}

	return 0;
}
#endif

} // namespace lexxos::apps::i2c_programs::picocalc_southbridge_detail
