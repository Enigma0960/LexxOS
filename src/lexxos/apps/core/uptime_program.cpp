#include <lexxos/apps/core/uptime_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::system_programs {

lexxos::program::ProgramDescriptor UptimeProgram::descriptor() {
	return {.name = "uptime", .help = "show system uptime", .handler = run, .legacy = false, .enabled = true};
}

int UptimeProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	writeUnsigned(context, uptimeMs(context));
	context.writeLine(" ms");
	return 0;
}

} // namespace lexxos::apps::system_programs
