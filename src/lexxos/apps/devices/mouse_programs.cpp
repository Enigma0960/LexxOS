#include <commands/command_selection.h>

#include <lexxos/apps/devices/mouse_programs.h>

#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/espidf/usb_hid_host.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec optionalMouseName[] = {
	    {
         .name = "name",
         .valueName = "name",
         .help = "pointing device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     }
    };

	constexpr lexxos::program::ProgramArgumentSpec mouseLiveArguments[] = {
	    {
         .name = "seconds",
         .shortName = 's',
         .valueName = "seconds",
         .help = "test duration in seconds",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "30",
	     },
	    {
         .name = "interval",
         .shortName = 'i',
         .valueName = "milliseconds",
         .help = "poll interval in milliseconds",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "25",
	     },
	    {
         .name = "name",
         .valueName = "name",
         .help = "pointing device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	void writeSigned(lexxos::os::AppContext& context, std::int32_t value) {
		if (value < 0) {
			context.write("-");
			auto const magnitude = static_cast<std::uint32_t>(-(static_cast<std::int64_t>(value)));
			writeUnsigned(context, magnitude);
		} else {
			writeUnsigned(context, static_cast<std::uint32_t>(value));
		}
	}

	void writeState(lexxos::os::AppContext& context, lexxos::devices::PointingDeviceState const& state) {
		context.write("connected=");
		context.write(state.connected ? "yes" : "no");
		context.write(" x=");
		writeSigned(context, state.x);
		context.write(" y=");
		writeSigned(context, state.y);
		context.write(" wheel=");
		writeSigned(context, state.wheel);
		context.write(" buttons=0x");
		writeHexByte(context, state.buttons);
		context.write(" [L=");
		context.write(state.buttonPressed(lexxos::devices::PointingButtonLeft) ? "1" : "0");
		context.write(" R=");
		context.write(state.buttonPressed(lexxos::devices::PointingButtonRight) ? "1" : "0");
		context.write(" M=");
		context.write(state.buttonPressed(lexxos::devices::PointingButtonMiddle) ? "1" : "0");
		context.write(" B=");
		context.write(state.buttonPressed(lexxos::devices::PointingButtonBack) ? "1" : "0");
		context.write(" F=");
		context.write(state.buttonPressed(lexxos::devices::PointingButtonForward) ? "1" : "0");
		context.write("] reports=");
		writeUnsigned(context, state.reportCount);
		context.write(" last=");
		writeUnsigned(context, state.lastReportTimestampMs);
		context.writeLine("ms");
	}

	lexxos::devices::PointingDevice* selectMouse(lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments, std::string_view command) {
		auto const name = arguments.positional("name");
		auto* mouse = name.empty() ? context.devices().primaryPointingDevice() : context.devices().pointingDevice(name);
		if (mouse == nullptr) {
			context.writeErrorLine(command == "mouse-info" ? "mouse-info: pointing device is not registered"
			        : command == "mouse-reset"             ? "mouse-reset: pointing device is not registered"
			                                               : "mouse-live: pointing device is not registered");
		}
		return mouse;
	}

} // namespace

#if LEXXOS_COMMAND_MOUSE_INFO
lexxos::program::ProgramDescriptor MouseInfoProgram::descriptor() {
	return {.name = "mouse-info", .help = "show USB mouse state", .description = "Prints connection, accumulated position, wheel, buttons and report counters.", .usage = "mouse-info [name]", .arguments = argumentList(optionalMouseName), .handler = run, .legacy = false, .enabled = true};
}
#endif

#if LEXXOS_COMMAND_MOUSE_INFO
int MouseInfoProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* mouse = selectMouse(context, arguments, "mouse-info");
	if (mouse == nullptr) {
		return 1;
	}
	writeState(context, mouse->state());
	return 0;
}
#endif

#if LEXXOS_COMMAND_MOUSE_RESET
lexxos::program::ProgramDescriptor MouseResetProgram::descriptor() {
	return {.name = "mouse-reset", .help = "reset USB mouse position", .description = "Resets accumulated X, Y and wheel values while preserving button and report state.", .usage = "mouse-reset [name]", .arguments = argumentList(optionalMouseName), .handler = run, .legacy = false, .enabled = true};
}
#endif

#if LEXXOS_COMMAND_MOUSE_RESET
int MouseResetProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* mouse = selectMouse(context, arguments, "mouse-reset");
	if (mouse == nullptr) {
		return 1;
	}
	auto lock = context.devices().lockPointingDevice(mouse->name(), lexxos::devices::DeviceResourcePriority::Normal, "mouse-reset");
	if (!lock) {
		context.writeErrorLine("mouse-reset: pointing device is busy");
		return 1;
	}
	mouse->resetPosition();
	context.writeLine("mouse position reset");
	return 0;
}
#endif

#if LEXXOS_COMMAND_MOUSE_LIVE
lexxos::program::ProgramDescriptor MouseLiveProgram::descriptor() {
	return {.name = "mouse-live", .help = "watch USB mouse activity", .description = "Prints mouse state whenever position, wheel, buttons or connection state changes.", .usage = "mouse-live [-s seconds] [-i milliseconds] [name]", .arguments = argumentList(mouseLiveArguments), .handler = run, .legacy = false, .enabled = true};
}
#endif

#if LEXXOS_COMMAND_MOUSE_LIVE
int MouseLiveProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* mouse = selectMouse(context, arguments, "mouse-live");
	if (mouse == nullptr) {
		return 1;
	}
	std::uint32_t seconds = 30, interval = 25;
	if (!parseUnsigned32(arguments.valueOr("seconds", "30"), seconds) || seconds == 0 || seconds > 3600) {
		context.writeErrorLine("mouse-live: invalid seconds value");
		return 1;
	}
	if (!parseUnsigned32(arguments.valueOr("interval", "25"), interval) || interval == 0 || interval > 1000) {
		context.writeErrorLine("mouse-live: invalid interval value");
		return 1;
	}
	auto lock = context.devices().lockPointingDevice(mouse->name(), lexxos::devices::DeviceResourcePriority::Normal, "mouse-live");
	if (!lock) {
		context.writeErrorLine("mouse-live: pointing device is busy");
		return 1;
	}
	auto previous = mouse->state();
	writeState(context, previous);
	auto const started = uptimeMs(context);
	while (uptimeMs(context) - started < seconds * 1000U) {
		if (context.pollCancellationRequest()) {
			context.writeLine("mouse-live: canceled");
			return 130;
		}
		auto const current = mouse->state();
		if (current.connected != previous.connected || current.x != previous.x || current.y != previous.y || current.wheel != previous.wheel || current.buttons != previous.buttons || current.reportCount != previous.reportCount) {
			writeState(context, current);
			previous = current;
		}
		lexxos::kernel::sleepMs(interval);
	}
	return 0;
}
#endif

#if LEXXOS_COMMAND_USB_HID_INFO
lexxos::program::ProgramDescriptor UsbHidInfoProgram::descriptor() {
	return {.name = "usb-hid-info", .help = "show USB HID host status", .description = "Prints USB Host/HID initialization, connection, event and error counters.", .usage = "usb-hid-info", .handler = run, .legacy = false, .enabled = true};
}
#endif

#if LEXXOS_COMMAND_USB_HID_INFO
int UsbHidInfoProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto const value = lexxos::devices::espidf_usb_hid::status();
	context.write("initialized=");
	context.write(value.initialized ? "yes" : "no");
	context.write(" usb-host=");
	context.write(value.usbHostInstalled ? "yes" : "no");
	context.write(" hid-driver=");
	context.write(value.hidDriverInstalled ? "yes" : "no");
	context.write(" keyboard=");
	context.write(value.keyboardConnected ? "connected" : "disconnected");
	context.write(" mouse=");
	context.write(value.mouseConnected ? "connected" : "disconnected");
	context.write(" gamepad=");
	context.write(value.gamepadConnected ? "connected" : "disconnected");
	context.write(" usb-info=");
	context.write(value.usbInfoAvailable ? "yes" : "no");
	context.write(" usb-info-error=");
	writeSigned(context, value.usbInfoError);
	context.write(" usb-devices=");
	writeUnsigned(context, value.usbDeviceCount);
	context.write(" usb-clients=");
	writeUnsigned(context, value.usbClientCount);
	context.write(" root-suspended=");
	context.write(value.rootPortSuspended ? "yes" : "no");
	context.write(" connects=");
	writeUnsigned(context, value.connectionEvents);
	context.write(" dropped=");
	writeUnsigned(context, value.droppedEvents);
	context.write(" errors=");
	writeUnsigned(context, value.errors);
	context.write(" last-error=");
	writeSigned(context, value.lastError);
	context.write(" gamepad-reports=");
	writeUnsigned(context, value.gamepadReports);
	context.write(" gamepad-last=");
	writeUnsigned(context, value.gamepadLastReportTimestampMs);
	context.write("ms gamepad-bytes=");
	writeUnsigned(context, value.gamepadLastReportBytes);
	context.write(" gamepad-report-id=0x");
	writeHexByte(context, value.gamepadReportId);
	context.writeLine("");
	return value.initialized ? 0 : 1;
}
#endif

} // namespace lexxos::apps::device_programs
