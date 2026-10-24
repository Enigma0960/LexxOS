#include <lexxos/apps/gameboy/gb_cpu_step_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbCpuStepProgram::descriptor() {
	return {
	    .name = "gb-cpu-step",
	    .help = "step Game Boy CPU core",
	    .description = "Opens a Game Boy ROM, attaches it to the bus and executes a bounded number of SM83 CPU instructions for diagnostics.",
	    .usage = "gb-cpu-step [--steps count] [--pc address] <rom>",
	    .arguments = argumentList(gbCpuStepArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbCpuStepProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, arguments.positional("rom"), path)) {
		return 1;
	}

	std::uint32_t steps = 8;
	if (!parseUnsigned(arguments.valueOr("steps", "8"), steps) || steps == 0) {
		context.writeErrorLine("gb-cpu-step: invalid step count");
		return 2;
	}
	if (steps > 200) {
		steps = 200;
	}

	std::uint32_t pc = 0x0100;
	if (!parseNumber(arguments.valueOr("pc", "0x0100"), pc) || pc > 0xFFFFU) {
		context.writeErrorLine("gb-cpu-step: invalid pc address");
		return 2;
	}

	auto* workspace = gameBoyWorkspace(context);
	if (workspace == nullptr) {
		return 1;
	}

	auto& cartridge = workspace->cartridge;
	if (!openCartridge(context, "gb-cpu-step", path.data(), cartridge)) {
		return 1;
	}

	auto& bus = workspace->bus;
	auto& cpu = workspace->cpu;

	bus.reset();
	bus.attachCartridge(&cartridge);
	cpu.resetToPostBootState();
	cpu.setPc(static_cast<std::uint16_t>(pc));

	auto const& info = cartridge.headerInfo();
	context.write("gb-cpu-step: loaded ");
	context.write(info.title.data()[0] != '\0' ? info.title.data() : "<empty>");
	context.write(" from ");
	context.writeLine(path.data());
	context.write("mapper: ");
	context.write(lexxos::gameboy::mapperName(cartridge.mapper()));
	context.write(" (");
	context.write(lexxos::gameboy::cartridgeTypeName(info.cartridgeType));
	context.write(", ");
	writeCode8(context, info.cartridgeType);
	context.writeLine(")");

	context.write("start: ");
	writeCpuState(context, cpu);
	context.writeLine("");

	std::uint32_t totalCycles = 0;
	lexxos::gameboy::CpuStepStatus lastStatus = lexxos::gameboy::CpuStepStatus::Ok;
	std::uint32_t executed = 0;
	for (std::uint32_t index = 0; index < steps; ++index) {
		auto const step = cpu.step(bus);
		bus.advanceCycles(step.cycles);
		totalCycles += step.cycles;
		lastStatus = step.status;
		writeCpuStep(context, index, step, cpu);
		++executed;
		if (step.status != lexxos::gameboy::CpuStepStatus::Ok) {
			break;
		}
	}

	context.write("cpu: ");
	context.write(lexxos::gameboy::toString(lastStatus));
	context.write(" steps=");
	writeUnsigned(context, executed);
	context.write(" cycles=");
	writeUnsigned(context, totalCycles);
	context.writeLine("");

	bus.attachCartridge(nullptr);
	if (lastStatus == lexxos::gameboy::CpuStepStatus::BusReadError) {
		return 1;
	}
	if (lastStatus == lexxos::gameboy::CpuStepStatus::UnsupportedOpcode) {
		return 2;
	}
	return 0;
}

} // namespace lexxos::apps::gameboy_programs
