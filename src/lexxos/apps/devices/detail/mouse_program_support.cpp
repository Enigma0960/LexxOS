#include <lexxos/apps/devices/detail/mouse_program_support.h>

#include <cstdint>
#include <string_view>

#include <commands/command_selection.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/espidf/usb_hid_host.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs::detail::mouse {

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

} // namespace lexxos::apps::device_programs::detail::mouse
