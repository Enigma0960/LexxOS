#include <lexxos/apps/core/help_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/program/program_manager.h>

namespace lexxos::apps::system_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec helpArguments[] = {
	    {.name = "program", .valueName = "program", .help = "program name to describe", .kind = lexxos::program::ProgramArgumentKind::Positional, .valueType = lexxos::program::ProgramArgumentValueType::String, .required = false},
	};

} // namespace

lexxos::program::ProgramDescriptor HelpProgram::descriptor() {
	return {.name = "help", .help = "show program list", .description = "Shows all registered LexxOS programs or detailed help for one selected program.", .usage = "help [program]", .arguments = argumentList(helpArguments), .handler = run, .legacy = false, .enabled = true};
}

int HelpProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const& programs = context.programs();
	auto const programName = arguments.positional("program");
	if (!programName.empty()) {
		auto const* program = programs.find(programName);
		if (program == nullptr) {
			context.write("unknown program: ");
			context.writeLine(programName);
			return 1;
		}
		programs.writeProgramHelp(context, *program);
		return 0;
	}
	programs.writeProgramList(context);
	return 0;
}

} // namespace lexxos::apps::system_programs
