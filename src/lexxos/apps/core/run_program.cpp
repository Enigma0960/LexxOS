#include <lexxos/apps/core/run_program.h>

#include <cstring>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_supervisor.h>

namespace lexxos::apps::system_programs {
namespace {

	bool equals(char const* value, char const* expected) {
		return value != nullptr && std::strcmp(value, expected) == 0;
	}

} // namespace

lexxos::program::ProgramDescriptor RunProgram::descriptor() {
	return {
	    .name = "run",
	    .help = "run a program through ProgramSupervisor",
	    .description = "Starts a program through the experimental ProgramSupervisor task path. Background jobs are quiet by default.",
	    .usage = "run [--bg] [--console] <program> [args...]",
	    .handler = run,
	    .allowExtraArguments = true,
	    .legacy = false,
	    .enabled = true,
	};
}

int RunProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	if (invocation.argc < 2) {
		context.writeErrorLine("usage: run <program> [args...]");
		return 2;
	}

	bool background = false;
	bool backgroundConsoleOutput = false;
	int programIndex = 1;

	while (programIndex < invocation.argc) {
		if (equals(invocation.argv[programIndex], "--bg") || equals(invocation.argv[programIndex], "-b")) {
			background = true;
			++programIndex;
			continue;
		}

		if (equals(invocation.argv[programIndex], "--console") || equals(invocation.argv[programIndex], "-c")) {
			backgroundConsoleOutput = true;
			++programIndex;
			continue;
		}

		if (equals(invocation.argv[programIndex], "--quiet") || equals(invocation.argv[programIndex], "-q")) {
			backgroundConsoleOutput = false;
			++programIndex;
			continue;
		}

		break;
	}

	if (invocation.argc <= programIndex) {
		context.writeErrorLine("usage: run [--bg] [--console] <program> [args...]");
		return 2;
	}

	lexxos::program::ProgramStartRequest request{
	    .name = invocation.argv[programIndex],
	    .argc = invocation.argc - programIndex,
	    .argv = &invocation.argv[programIndex],
	};

	if (background && !backgroundConsoleOutput) {
		request.standardOutput = &lexxos::io::nullOutputStream();
		request.standardError = &lexxos::io::nullOutputStream();
	}

	if (background) {
		auto const result = lexxos::program::supervisor().startBackground(context, request);
		if (!result.started()) {
			context.writeError("run: failed to start background process: ");
			context.writeErrorLine(lexxos::program::toString(result.status));
			return 1;
		}

		context.write("run: started background process ");
		writeUnsigned(context, result.id);
		context.write(" (");
		context.write(request.name);
		context.writeLine(")");
		return 0;
	}

	auto const result = lexxos::program::supervisor().startForeground(context, request);
	if (!result.started()) {
		context.writeError("run: failed to start process: ");
		context.writeErrorLine(lexxos::program::toString(result.status));
		return 1;
	}

	context.write("run: started foreground process ");
	writeUnsigned(context, result.id);
	context.write(" (");
	context.write(request.name);
	context.writeLine(")");
	return 0;
}

} // namespace lexxos::apps::system_programs
