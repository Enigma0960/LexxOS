#include <lexxos/apps/gameboy/gb_test_suite_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbTestSuiteProgram::descriptor() {
	return {
	    .name = "gb-test-suite",
	    .help = "run Blargg Game Boy CPU individual test suite",
	    .description = "Runs the 11 ROM files from gb-test-roms/cpu_instrs/individual and prints a compact pass/fail summary.",
	    .usage = "gb-test-suite [--cycles count] [--trace] [--stop-on-fail] <dir>",
	    .arguments = argumentList(gbTestSuiteArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbTestSuiteProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::array<char, lexxos::vfs::maxPathLength> directory{};
	if (!resolvePath(context, arguments.positional("dir"), directory)) {
		return 1;
	}

	lexxos::vfs::Stat stat{};
	if (!lexxos::vfs::stat(directory.data(), stat)) {
		context.write("gb-test-suite: cannot access: ");
		context.writeLine(directory.data());
		return 1;
	}
	if (stat.type != lexxos::vfs::NodeType::Directory) {
		context.write("gb-test-suite: not a directory: ");
		context.writeLine(directory.data());
		return 1;
	}

	std::uint32_t cycleLimit = defaultGbTestSuiteCycles;
	if (!parseUnsigned(arguments.valueOr("cycles", "143818752"), cycleLimit) || cycleLimit == 0) {
		context.writeErrorLine("gb-test-suite: invalid cycles count");
		return 2;
	}
	if (cycleLimit > maxGbTestCycles) {
		cycleLimit = maxGbTestCycles;
	}

	auto const trace = arguments.has("trace");
	auto const stopOnFail = arguments.has("stop-on-fail");
	std::uint32_t passed = 0;
	std::uint32_t failed = 0;
	std::uint32_t incomplete = 0;
	std::uint32_t stopped = 0;
	std::uint32_t errors = 0;
	int finalExitCode = 0;

	context.write("gb-test-suite: dir=");
	context.write(directory.data());
	context.write(" cycles/test=");
	writeUnsigned(context, cycleLimit);
	context.write(" tests=");
	writeUnsigned(context, static_cast<std::uint32_t>(sizeof(blarggCpuIndividualRoms) / sizeof(blarggCpuIndividualRoms[0])));
	context.writeLine("");

	for (auto const romName : blarggCpuIndividualRoms) {
		std::array<char, lexxos::vfs::maxPathLength> romPath{};
		if (!joinPath(directory.data(), romName, romPath)) {
			context.write("gb-test-suite: path too long: ");
			context.writeLine(romName);
			++errors;
			finalExitCode = 1;
			if (stopOnFail) {
				break;
			}
			continue;
		}

		GbTestRunSummary summary{};
		auto const runOk = runGbTestRom(context, "gb-test-suite", romPath.data(), cycleLimit, trace, trace, summary);
		auto const exitCode = gbTestExitCode(summary);
		if (exitCode != 0 && finalExitCode == 0) {
			finalExitCode = exitCode;
		}

		context.write("gb-test-suite: ");
		context.write(romName);
		context.write(" result=");
		context.write(gbTestResultName(summary));
		context.write(" steps=");
		writeUnsigned(context, summary.total.steps);
		context.write(" cycles=");
		writeUnsigned(context, summary.total.cycles);
		context.write(" status=");
		context.write(lexxos::gameboy::toString(summary.total.status));
		context.write(" serial=");
		writeUnsigned(context, summary.serialSize);
		context.write(" bank=");
		writeUnsigned(context, summary.selectedBank);
		context.write(" cache=");
		writeUnsigned(context, summary.cacheFills);
		context.write("/");
		writeUnsigned(context, summary.cacheRetries);
		context.write("/");
		writeUnsigned(context, summary.cacheFailures);
		context.writeLine("");

		if (!isContinuableEmulationStatus(summary.total.status)) {
			writeRunStopStep(context, "gb-test-suite: ", summary.total);
		}

		if (!summary.passed && !summary.failed && isContinuableEmulationStatus(summary.total.status) && summary.total.cycles >= cycleLimit) {
			context.write("gb-test-suite: ");
			context.write(romName);
			context.writeLine(": cycle budget exhausted before serial Passed/Failed");
		}

		if (summary.passed) {
			++passed;
		} else if (summary.failed) {
			++failed;
		} else if (!runOk || !summary.opened || !summary.mapperSupported || summary.total.status == lexxos::gameboy::CpuStepStatus::BusReadError) {
			++errors;
		} else if (!isContinuableEmulationStatus(summary.total.status)) {
			++stopped;
		} else {
			++incomplete;
		}

		if (stopOnFail && exitCode != 0) {
			break;
		}
	}

	context.write("gb-test-suite: summary passed=");
	writeUnsigned(context, passed);
	context.write(" failed=");
	writeUnsigned(context, failed);
	context.write(" incomplete=");
	writeUnsigned(context, incomplete);
	context.write(" stopped=");
	writeUnsigned(context, stopped);
	context.write(" errors=");
	writeUnsigned(context, errors);
	context.writeLine("");
	context.write("gb-test-suite: result=");
	context.writeLine(finalExitCode == 0 ? "passed" : "failed");
	return finalExitCode;
}

} // namespace lexxos::apps::gameboy_programs
