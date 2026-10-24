#include <lexxos/apps/utility/echo_program.h>

#include <lexxos/os/app_context.h>

namespace lexxos::apps::utility_programs {

lexxos::program::ProgramDescriptor EchoProgram::descriptor() {
	return {
	    .name = "echo",
	    .help = "print program arguments",
	    .description = "Prints all positional arguments separated by spaces.",
	    .usage = "echo [text ...]",
	    .handler = run,
	    .allowExtraArguments = true,
	    .legacy = false,
	    .enabled = true,
	};
}

int EchoProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	for (int index = 1; index < invocation.argc; ++index) {
		if (index > 1) {
			context.write(" ");
		}
		context.write(invocation.argv[index]);
	}
	context.writeLine("");
	return 0;
}

} // namespace lexxos::apps::utility_programs
