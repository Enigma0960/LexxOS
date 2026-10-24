#include <lexxos/apps/gameboy/gb_test_run_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbTestRunProgram::descriptor() {
	return {
	    .name = "gb-test-run",
	    .help = "run Game Boy hardware test ROM with serial capture",
	    .description = "Loads a Game Boy test ROM, runs it for a bounded cycle budget and prints the captured FF01/FF02 serial output used by many Blargg test ROMs.",
	    .usage = "gb-test-run [--cycles count] [--trace] <rom>",
	    .arguments = argumentList(gbTestRunArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbTestRunProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, arguments.positional("rom"), path)) {
		return 1;
	}

	std::uint32_t cycleLimit = defaultGbTestCycles;
	if (!parseUnsigned(arguments.valueOr("cycles", "2247168"), cycleLimit) || cycleLimit == 0) {
		context.writeErrorLine("gb-test-run: invalid cycles count");
		return 2;
	}
	if (cycleLimit > maxGbTestCycles) {
		cycleLimit = maxGbTestCycles;
	}

	GbTestRunSummary summary{};
	runGbTestRom(context, "gb-test-run", path.data(), cycleLimit, arguments.has("trace"), true, summary);

	context.write("gb-test-run: result=");
	context.writeLine(gbTestResultName(summary));
	return gbTestExitCode(summary);
}

} // namespace lexxos::apps::gameboy_programs
