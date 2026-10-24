#include <lexxos/apps/utility/reboot_program.h>

#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::apps::utility_programs {

lexxos::program::ProgramDescriptor RebootProgram::descriptor() {
	return {
	    .name = "reboot",
	    .help = "reboot target device",
	    .usage = "reboot",
	    .handler = run,
	};
}

int RebootProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto const& power = context.platform().power;
	if (power.reboot == nullptr) {
		context.writeLine("reboot unavailable");
		return 1;
	}

	context.writeLine("reboot requested");
	power.reboot();
	return 0;
}

} // namespace lexxos::apps::utility_programs
