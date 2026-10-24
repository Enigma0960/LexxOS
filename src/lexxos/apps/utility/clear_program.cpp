#include <lexxos/apps/utility/clear_program.h>

#include <lexxos/app/console_output.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::utility_programs {

lexxos::program::ProgramDescriptor ClearProgram::descriptor() {
	return {
	    .name = "clear",
	    .help = "clear ANSI terminal",
	    .description = "Clears the current ANSI-compatible terminal screen.",
	    .usage = "clear",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int ClearProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	lexxos::app::console_output::clear(context.platform());
	return 0;
}

} // namespace lexxos::apps::utility_programs
