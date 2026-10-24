#include <commands/command_selection.h>

#include <lexxos/apps/picocalc/picocalc_info_program.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/detail/i2c_program_support.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/picocalc/picocalc_southbridge.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::apps::i2c_programs {

using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentList;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;

namespace {

	template <std::size_t Size>
	constexpr ProgramArgumentList argumentList(ProgramArgumentSpec const (&arguments)[Size]) {
		return ProgramArgumentList{
		    .data = arguments,
		    .size = Size,
		};
	}

	constexpr ProgramArgumentSpec picoCalcInfoArguments[] = {
	    {
         .name = "bus",
         .shortName = 'b',
         .valueName = "name",
         .help = "I2C bus name",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "debug",
	     },
	    {
         .name = "address",
         .shortName = 'a',
         .valueName = "addr",
         .help = "southbridge 7-bit I2C address",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0x1F",
	     },
	    {
         .name = "force",
         .shortName = 'f',
         .help = "deprecated alias for --direct-i2c",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "direct-i2c",
         .help = "diagnostic-only direct reads when no southbridge service is registered",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	};

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

	std::uint32_t uptimeMs(lexxos::os::AppContext& context) {
		auto const& clock = context.platform().clock;
		if (clock.uptimeMs == nullptr) {
			return 0;
		}
		return clock.uptimeMs();
	}

	lexxos::devices::PicoCalcSouthbridgeRuntimeService* picoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::service(context.devices());
	}

	lexxos::devices::DeviceResourceLock lockPicoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(
		    context.devices(),
		    "picocalc-info",
		    lexxos::devices::DeviceResourcePriority::Interactive);
	}

	bool directI2cRequested(lexxos::program::ParsedArguments const& arguments) {
		return arguments.has("direct-i2c") || arguments.has("force");
	}

	void writeOwnedSouthbridgeNotice(lexxos::os::AppContext& context) {
		context.writeLine("picocalc-info: skipped while picocalc-keyboard owns the southbridge I2C device");
		context.writeLine("picocalc-info: direct reads are disabled while normal keyboard input is active");
		context.writeLine("picocalc-info: use power/backlight for safe service-backed power and backlight requests");
	}

	void writeDirectI2cRequired(lexxos::os::AppContext& context) {
		context.writeErrorLine("picocalc-info: no registered picocalc-southbridge service is available");
		context.writeErrorLine("picocalc-info: direct I2C diagnostics require explicit --direct-i2c");
		context.writeErrorLine("picocalc-info: use --direct-i2c only during bring-up without active picocalc-keyboard");
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

		context.write(name);
		context.write(": ");
		detail::writeUnsigned(context, static_cast<std::uint32_t>(value));
		context.write(" (");
		detail::writeHex8(context, value);
		context.writeLine(")");
	}

	void writeSnapshotBattery(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot) {
		if (!snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validBattery)) {
			context.writeLine("battery: read failed");
			return;
		}

		context.write("battery: raw=");
		detail::writeHex8(context, snapshot.battery.raw);
		context.write(" charging=");
		context.write(snapshot.battery.charging() ? "yes" : "no");
		context.write(" value=");
		detail::writeUnsigned(context, static_cast<std::uint32_t>(snapshot.battery.value()));
		context.writeLine("");
	}

	void writeSnapshotKeyboardState(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot) {
		if (!snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validKeyboardState)) {
			context.writeLine("keyboard-state: read failed");
			return;
		}

		context.write("keyboard-state: raw=");
		writeHex16(context, snapshot.keyboardState.raw());
		context.write(" state=");
		detail::writeHex8(context, snapshot.keyboardState.state);
		context.write(" value=");
		detail::writeHex8(context, snapshot.keyboardState.value);
		context.write(" fifo=");
		detail::writeUnsigned(context, static_cast<std::uint32_t>(snapshot.keyboardState.fifoCount()));
		context.write(" legacy-fifo=");
		detail::writeUnsigned(context, static_cast<std::uint32_t>(snapshot.keyboardState.legacyFifoCount()));
		context.write(" caps=");
		context.write(snapshot.keyboardState.capsLock() ? "on" : "off");
		context.write(" num=");
		context.write(snapshot.keyboardState.numLock() ? "on" : "off");
		context.writeLine("");
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
		writeSnapshotByte(context, snapshot, "power-off-supported", Snapshot::validPowerOffSupport, snapshot.powerOffSupport);
	}

	void writeKeyboardCachedSnapshotReport(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot) {
		writeSnapshotReport(context, snapshot);
	}

	void writeKeyboardCachedSnapshotMetadata(lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService const& service,
	    lexxos::devices::PicoCalcSouthbridgeSnapshot const& snapshot,
	    std::uint32_t snapshotMs) {
		context.write("keyboard-ready: ");
		context.writeLine(service.isReady() ? "yes" : "no");

		context.write("snapshot-ms: ");
		detail::writeUnsigned(context, snapshotMs);
		context.writeLine("");

		context.write("snapshot-full-refresh-ms: ");
		detail::writeUnsigned(context, service.southbridgeFullSnapshotMs());
		context.writeLine("");

		context.write("snapshot-last-refresh-ok: ");
		context.writeLine(service.southbridgeLastRefreshOk() ? "yes" : "no");

		auto const nowMs = uptimeMs(context);
		context.write("snapshot-age-ms: ");
		if (snapshotMs == 0 || nowMs == 0) {
			context.writeLine("unknown");
		} else {
			detail::writeUnsigned(context, nowMs - snapshotMs);
			context.writeLine("");
		}

		context.write("snapshot-fields: ");
		detail::writeUnsigned(context, snapshot.validFieldCount());
		context.writeLine("");

		context.write("snapshot-updates: ");
		detail::writeUnsigned(context, service.southbridgeSnapshotUpdates());
		context.writeLine("");

		context.write("snapshot-failures: ");
		detail::writeUnsigned(context, service.southbridgeSnapshotFailures());
		context.writeLine("");

		context.write("stale-event-releases: ");
		detail::writeUnsigned(context, service.staleReleaseCount());
		context.writeLine("");

		context.write("stale-scan-code-releases: ");
		detail::writeUnsigned(context, service.staleScanCodeReleases());
		context.writeLine("");

		context.write("raw-state: ");
		writeHex16(context, static_cast<std::uint16_t>(service.rawState() & 0xFFFFU));
		context.write(" keys=");
		detail::writeUnsigned(context, service.rawKeyCount());
		context.writeLine("");
	}

} // namespace

ProgramDescriptor PicoCalcInfoProgram::descriptor() {
	return {
	    .name = "picocalc-info",
	    .help = "show PicoCalc southbridge information",
	    .description = "Shows PicoCalc STM32 southbridge status through the registered service, with explicit direct-I2C diagnostics for bring-up.",
	    .usage = "picocalc-info [--direct-i2c] [--bus <name>] [--address <addr>]",
	    .arguments = argumentList(picoCalcInfoArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int PicoCalcInfoProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const directRequested = directI2cRequested(arguments);
	auto* service = picoCalcSouthbridge(context);
	if (service != nullptr) {
		if (directRequested) {
			context.writeLine("picocalc-info: direct I2C diagnostics refused while picocalc-keyboard is active");
			context.writeLine("picocalc-info: direct southbridge reads can wedge the STM32 keyboard controller");
		}

		auto serviceLock = lockPicoCalcSouthbridge(context);
		if (!serviceLock) {
			context.writeErrorLine("picocalc-info: southbridge service is busy");
			return 1;
		}

		lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
		if (service->southbridgeSnapshot(snapshot)) {
			context.writeLine("PicoCalc southbridge info");
			context.writeLine("source: picocalc-southbridge service cached snapshot");
			context.writeLine("direct-i2c: no");
			writeKeyboardCachedSnapshotMetadata(context, *service, snapshot, service->southbridgeSnapshotMs());
			writeKeyboardCachedSnapshotReport(context, snapshot);
			return 0;
		}

		writeOwnedSouthbridgeNotice(context);
		context.writeLine("picocalc-info: no cached keyboard-state snapshot is available yet");
		return 0;
	}

#if LEXXOS_COMMAND_TEST_OPERATIONS
	if (!directRequested) {
		writeDirectI2cRequired(context);
		return 1;
	}

	auto const busName = arguments.valueOr("bus", "debug");
	std::uint32_t addressValue = lexxos::devices::PicoCalcSouthbridge::defaultAddress;
	if (!detail::parseUnsigned(arguments.valueOr("address", "0x1F"), addressValue) || addressValue > 0x7FU) {
		context.writeErrorLine("picocalc-info: invalid --address value, expected 7-bit I2C address");
		return 2;
	}

	auto* bus = context.devices().i2cBus(busName);
	if (bus == nullptr) {
		context.writeError("picocalc-info: unknown I2C bus: ");
		context.writeErrorLine(busName);
		context.writeErrorLine("available buses:");
		detail::writeI2cBusList(context, true);
		return 1;
	}

	auto busLock = context.devices().lockI2cBus(busName, lexxos::devices::DeviceResourcePriority::Interactive, "picocalc-info");
	if (!busLock) {
		context.writeError("picocalc-info: I2C bus is busy: ");
		context.writeErrorLine(busName);
		return 1;
	}

	lexxos::devices::PicoCalcSouthbridge southbridge(*bus, static_cast<std::uint8_t>(addressValue));
	if (!southbridge.isReady()) {
		context.writeError("picocalc-info: I2C bus is not ready: ");
		context.writeErrorLine(busName);
		return 1;
	}

	context.writeLine("PicoCalc southbridge info");
	context.writeLine("source: direct-i2c diagnostic");
	context.writeLine("direct-i2c: yes");
	context.write("bus: ");
	context.writeLine(busName);
	context.write("address: ");
	detail::writeHex8(context, southbridge.address());
	context.writeLine("");
	context.write("probe: ");
	context.writeLine(southbridge.probe() ? "yes" : "no");

	lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
	if (!southbridge.readSnapshot(snapshot)) {
		context.writeErrorLine("picocalc-info: southbridge did not respond to snapshot reads");
		return 1;
	}
	writeSnapshotReport(context, snapshot);

	return 0;
#else
	context.writeErrorLine("picocalc-info: direct I2C tests are disabled in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::i2c_programs
