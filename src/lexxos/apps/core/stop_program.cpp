#include <lexxos/apps/core/stop_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_supervisor.h>

namespace lexxos::apps::system_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec stopArguments[] = {
	    {
         .name = "id",
         .valueName = "id",
         .help = "supervised process id",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = true,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor StopProgram::descriptor() {
	return {
	    .name = "stop",
	    .help = "request supervised process stop",
	    .description = "Requests cooperative cancellation for a process created by ProgramSupervisor.",
	    .usage = "stop <id>",
	    .arguments = argumentList(stopArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int StopProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::uint32_t id = 0;
	if (!parseUnsigned32(arguments.positional("id"), id) || id == 0) {
		context.writeErrorLine("stop: invalid process id");
		return 2;
	}

	if (!lexxos::program::supervisor().requestStop(id)) {
		context.writeError("stop: process is not running: ");
		writeUnsigned(context, id);
		context.writeErrorLine("");
		return 1;
	}

	context.write("stop: requested process ");
	writeUnsigned(context, id);
	context.writeLine("");
	return 0;
}

} // namespace lexxos::apps::system_programs
