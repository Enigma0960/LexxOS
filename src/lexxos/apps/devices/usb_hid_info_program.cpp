#include <lexxos/apps/devices/usb_hid_info_program.h>

#include <cstdint>
#include <string_view>

#include <commands/command_selection.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/mouse_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/espidf/usb_hid_host.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {

using detail::mouse::writeSigned;

#if LEXXOS_COMMAND_USB_HID_INFO

namespace {

} // namespace

lexxos::program::ProgramDescriptor UsbHidInfoProgram::descriptor() {
	return {.name = "usb-hid-info", .help = "show USB HID host status", .description = "Prints USB Host/HID initialization, connection, event and error counters.", .usage = "usb-hid-info", .handler = run, .legacy = false, .enabled = true};
}

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
