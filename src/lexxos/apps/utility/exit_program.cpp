#include <lexxos/apps/utility/exit_program.h>

#include <lexxos/kernel/system.h>
#include <lexxos/os/app_context.h>
#include <lexxos/shell/shell.h>

namespace lexxos::apps::utility_programs {

lexxos::program::ProgramDescriptor ExitProgram::descriptor() {
	return {
	    .name = "exit",
	    .help = "exit host simulation",
	    .usage = "exit",
	    .handler = run,
	};
}

int ExitProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
#if defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	if (auto* shell = context.shell(); shell != nullptr) {
		shell->requestStop();
	}
	lexxos::kernel::requestShutdown();
#else
	context.writeLine("exit is available only on host or simulator platform");
#endif
	return 0;
}

} // namespace lexxos::apps::utility_programs
