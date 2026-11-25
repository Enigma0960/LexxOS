#include <lexxos/apps/devices/touch_info_program.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <commands/command_selection.h>

#include <lexxos/app/picocalc_display_power.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/apps/devices/detail/touch_program_support.h>
#include <lexxos/apps/devices/touch_programs.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/runtime/capabilities.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::device_programs {

using detail::touch::selectTouch;
using detail::touch::writeCalibration;
using detail::touch::writeSigned;

#if LEXXOS_COMMAND_TOUCH_INFO

namespace {

	constexpr lexxos::program::ProgramArgumentSpec optionalTouchName[] = {
	    {
	     .name = "name",
	     .valueName = "name",
	     .help = "touch-capable pointing device name",
	     .kind = lexxos::program::ProgramArgumentKind::Positional,
	     .valueType = lexxos::program::ProgramArgumentValueType::String,
	     .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor TouchInfoProgram::descriptor() {
	return {
	    .name = "touch-info",
	    .help = "show touch panel state",
	    .description = "Prints touch-capable pointing device contacts, raw coordinates and calibration.",
	    .usage = "touch-info [name]",
	    .arguments = argumentList(optionalTouchName),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int TouchInfoProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* touch = selectTouch(context, arguments.positional("name"), "touch-info");
	if (touch == nullptr) {
		return 1;
	}

	auto state = touch->touchState();
	context.write("device: ");
	context.write(touch->name());
	context.write(" connected=");
	context.write(state.connected ? "yes" : "no");
	context.write(" contacts=");
	writeUnsigned(context, state.contactCount);
	context.write("/");
	writeUnsigned(context, state.maxContacts);
	context.write(" reports=");
	writeUnsigned(context, state.reportCount);
	context.write(" last=");
	writeUnsigned(context, state.lastReportTimestampMs);
	context.writeLine("ms");
	for (std::uint8_t index = 0; index < state.contactCount && index < lexxos::devices::maxTouchContacts; ++index) {
		auto const& contact = state.contacts[index];
		context.write("contact[");
		writeUnsigned(context, index);
		context.write("]: id=");
		writeUnsigned(context, contact.id);
		context.write(" x=");
		writeSigned(context, contact.x);
		context.write(" y=");
		writeSigned(context, contact.y);
		context.write(" raw=");
		writeSigned(context, contact.rawX);
		context.write(",");
		writeSigned(context, contact.rawY);
		context.write(" size=");
		writeUnsigned(context, contact.size);
		context.writeLine("");
	}

	writeCalibration(context, state.calibration);
	context.write("path: ");
	context.writeLine(touchCalibrationConfigPath);
	return 0;
}

#endif

} // namespace lexxos::apps::device_programs
