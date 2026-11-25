#include <lexxos/apps/devices/mouse_info_program.h>

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
using detail::mouse::writeState;

#if LEXXOS_COMMAND_MOUSE_INFO

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

lexxos::program::ProgramDescriptor MouseInfoProgram::descriptor() {
	return {.name = "mouse-info", .help = "show USB mouse state", .description = "Prints connection, accumulated position, wheel, buttons and report counters.", .usage = "mouse-info [name]", .arguments = argumentList(optionalMouseName), .handler = run, .legacy = false, .enabled = true};
}

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

} // namespace lexxos::apps::device_programs
