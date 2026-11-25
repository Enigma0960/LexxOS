#include <lexxos/apps/devices/mouse_live_program.h>

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

#if LEXXOS_COMMAND_MOUSE_LIVE

namespace {

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

} // namespace

lexxos::program::ProgramDescriptor MouseLiveProgram::descriptor() {
	return {.name = "mouse-live", .help = "watch USB mouse activity", .description = "Prints mouse state whenever position, wheel, buttons or connection state changes.", .usage = "mouse-live [-s seconds] [-i milliseconds] [name]", .arguments = argumentList(mouseLiveArguments), .handler = run, .legacy = false, .enabled = true};
}

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

} // namespace lexxos::apps::device_programs
