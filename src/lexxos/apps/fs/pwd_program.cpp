#include <lexxos/apps/fs/pwd_program.h>

#include <lexxos/os/app_context.h>

namespace lexxos::apps::filesystem_programs {

lexxos::program::ProgramDescriptor PwdProgram::descriptor() {
	return {
	    .name = "pwd",
	    .help = "show current VFS directory",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int PwdProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	context.writeLine(context.workingDirectory());
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
