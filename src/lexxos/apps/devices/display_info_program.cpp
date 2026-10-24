#include <lexxos/apps/devices/display_info_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec optionalNameArgument[] = {
	    {
         .name = "name",
         .valueName = "name",
         .help = "device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor DisplayInfoProgram::descriptor() {
	return {
	    .name = "display-info",
	    .help = "show display device information",
	    .description = "Shows display size and native pixel format. Without a name it uses the primary display.",
	    .usage = "display-info [name]",
	    .arguments = argumentList(optionalNameArgument),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int DisplayInfoProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* display = detail::selectedDisplay(context, arguments.positional("name"));
	if (display == nullptr) {
		context.writeErrorLine("display-info: display device is not registered");
		return 1;
	}

	context.write("name: ");
	context.writeLine(display->name());
	context.write("ready: ");
	context.writeLine(display->isReady() ? "yes" : "no");
	context.write("size: ");
	writeUnsigned(context, static_cast<std::uint32_t>(display->width()));
	context.write("x");
	writeUnsigned(context, static_cast<std::uint32_t>(display->height()));
	context.writeLine("");
	context.write("format: ");
	context.writeLine(lexxos::devices::toString(display->nativeFormat()));
	context.write("power-mode: ");
	if (display->supportsPowerMode()) {
		context.writeLine(lexxos::devices::toString(display->powerMode()));
	} else {
		context.writeLine("unsupported");
	}
	return 0;
}

} // namespace lexxos::apps::device_programs
