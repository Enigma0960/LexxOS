#include <lexxos/apps/devices/mouse_reset_program.h>

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

using detail::mouse::selectMouse;

#if LEXXOS_COMMAND_MOUSE_RESET

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

} // namespace

lexxos::program::ProgramDescriptor MouseResetProgram::descriptor() {
	return {.name = "mouse-reset", .help = "reset USB mouse position", .description = "Resets accumulated X, Y and wheel values while preserving button and report state.", .usage = "mouse-reset [name]", .arguments = argumentList(optionalMouseName), .handler = run, .legacy = false, .enabled = true};
}

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

} // namespace lexxos::apps::device_programs
