#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

#include <cstring>

namespace lexxos::apps::gameboy_programs {

bool serialOutputContains(lexxos::gameboy::Bus const& bus, char const* needle) {
	if (needle == nullptr || needle[0] == '\0') {
		return false;
	}

	for (std::size_t offset = 0; offset < bus.serialOutputSize(); ++offset) {
		std::size_t matched = 0;
		while (needle[matched] != '\0' && offset + matched < bus.serialOutputSize() && bus.serialOutputByte(offset + matched) == static_cast<std::uint8_t>(needle[matched])) {
			++matched;
		}
		if (needle[matched] == '\0') {
			return true;
		}
	}
	return false;
}

void writeSerialOutput(lexxos::os::AppContext& context, lexxos::gameboy::Bus const& bus) {
	context.write("serial-size=");
	writeUnsigned(context, static_cast<std::uint32_t>(bus.serialOutputSize()));
	context.write(" overflow=");
	context.write(bus.serialOutputOverflowed() ? "1" : "0");
	context.writeLine("");
	context.writeLine("serial-output:");
	for (std::size_t index = 0; index < bus.serialOutputSize(); ++index) {
		auto const value = bus.serialOutputByte(index);
		if (value == '\n') {
			context.writeLine("");
		} else if (value == '\r') {
			// Ignore CR so CRLF output remains readable on the shell.
		} else if (value >= 0x20 && value < 0x7F) {
			char text[] = {static_cast<char>(value), '\0'};
			context.write(text);
		} else {
			context.write("\\x");
			writeCode8(context, value);
		}
	}
	context.writeLine("");
}

std::string_view gbTestResultName(GbTestRunSummary const& summary) {
	if (!summary.opened) {
		return "open-error";
	}
	if (!summary.mapperSupported) {
		return "unsupported-mapper";
	}
	if (summary.passed) {
		return "passed";
	}
	if (summary.failed) {
		return "failed";
	}
	if (!isContinuableEmulationStatus(summary.total.status)) {
		return "stopped";
	}
	return "incomplete";
}

int gbTestExitCode(GbTestRunSummary const& summary) {
	if (!summary.opened) {
		return 1;
	}
	if (!summary.mapperSupported) {
		return 2;
	}
	if (summary.failed || summary.total.status == lexxos::gameboy::CpuStepStatus::BusReadError) {
		return 1;
	}
	if (summary.total.status == lexxos::gameboy::CpuStepStatus::UnsupportedOpcode) {
		return 2;
	}
	return summary.passed ? 0 : 2;
}

void writeGbTestRunDetails(
    lexxos::os::AppContext& context,
    std::string_view programName,
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::Cpu const& cpu,
    lexxos::gameboy::Cartridge const& cartridge,
    GbTestRunSummary const& summary,
    RunTraceBuffer const& trace) {
	auto const* prefix = programName == "gb-test-suite" ? "gb-test-suite: " : "gb-test-run: ";
	context.write(programName);
	context.write(": steps=");
	writeUnsigned(context, summary.total.steps);
	context.write(" cycles=");
	writeUnsigned(context, summary.total.cycles);
	context.write(" status=");
	context.write(lexxos::gameboy::toString(summary.total.status));
	context.writeLine("");
	writeRunStopStep(context, prefix, summary.total);
	writeCpuState(context, cpu);
	context.writeLine("");
	writeBusIoSnapshot(context, bus, programName == "gb-test-suite" ? "gb-test-suite: io " : "gb-test-run: io ");
	writeCartridgeCacheStats(context, cartridge, prefix);
	writeRunTrace(context, prefix, trace);
}

bool runGbTestRom(
    lexxos::os::AppContext& context,
    std::string_view programName,
    std::string_view path,
    std::uint32_t cycleLimit,
    bool traceEnabled,
    bool verbose,
    GbTestRunSummary& summary) {
	auto* workspace = gameBoyWorkspace(context);
	if (workspace == nullptr) {
		return false;
	}

	auto& cartridge = workspace->cartridge;
	if (!openCartridge(context, programName, path, cartridge)) {
		return false;
	}
	summary.opened = true;
	if (!cartridge.mapperSupported()) {
		context.write(programName);
		context.writeLine(": cartridge mapper is not supported yet");
		return false;
	}
	summary.mapperSupported = true;

	auto& bus = workspace->bus;
	auto& cpu = workspace->cpu;

	bus.reset();
	bus.attachCartridge(&cartridge);
	bus.clearSerialOutput();
	cpu.resetToPostBootState();

	auto const& info = cartridge.headerInfo();
	if (verbose) {
		context.write(programName);
		context.write(": loaded ");
		context.write(info.title.data()[0] != '\0' ? info.title.data() : "<empty>");
		context.write(" from ");
		context.writeLine(path);
		context.write(programName);
		context.write(": mapper ");
		context.writeLine(lexxos::gameboy::mapperName(cartridge.mapper()));
	}

	RunTraceBuffer trace{};
	RunBudgetResult total{};
	while (total.cycles < cycleLimit && isContinuableEmulationStatus(total.status)) {
		auto const remaining = cycleLimit - total.cycles;
		auto const chunkCycles = remaining < 16384U ? remaining : 16384U;
		auto const chunk = runCpuBudget(cpu, bus, 2048, chunkCycles, traceEnabled ? &trace : nullptr, &cartridge);
		total.steps += chunk.steps;
		total.cycles += chunk.cycles;
		total.status = chunk.status;
		total.lastStep = chunk.lastStep;
		total.hasLastStep = chunk.hasLastStep;
		if (serialOutputContains(bus, "Passed") || serialOutputContains(bus, "Failed") || chunk.steps == 0) {
			break;
		}
	}

	summary.total = total;
	summary.passed = serialOutputContains(bus, "Passed");
	summary.failed = serialOutputContains(bus, "Failed");
	summary.serialSize = static_cast<std::uint32_t>(bus.serialOutputSize());
	summary.serialOverflow = bus.serialOutputOverflowed();
	summary.cacheFills = cartridge.cacheFillCount();
	summary.cacheRetries = cartridge.cacheRetryCount();
	summary.cacheFailures = cartridge.cacheFailureCount();
	summary.selectedBank = cartridge.selectedRomBank();

	if (verbose) {
		writeGbTestRunDetails(context, programName, bus, cpu, cartridge, summary, trace);
		writeSerialOutput(context, bus);
	}

	bus.attachCartridge(nullptr);
	return true;
}

bool joinPath(std::string_view directory, std::string_view filename, std::array<char, lexxos::vfs::maxPathLength>& output) {
	if (directory.empty() || filename.empty()) {
		return false;
	}

	auto const needsSeparator = directory[directory.size() - 1U] != '/';
	auto const length = directory.size() + (needsSeparator ? 1U : 0U) + filename.size();
	if (length + 1U > output.size()) {
		return false;
	}

	std::size_t position = 0;
	std::memcpy(output.data() + position, directory.data(), directory.size());
	position += directory.size();
	if (needsSeparator) {
		output[position++] = '/';
	}
	std::memcpy(output.data() + position, filename.data(), filename.size());
	position += filename.size();
	output[position] = '\0';
	return true;
}

} // namespace lexxos::apps::gameboy_programs
