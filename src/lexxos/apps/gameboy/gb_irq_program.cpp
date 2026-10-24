#include <lexxos/apps/gameboy/gb_irq_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbIrqProgram::descriptor() {
	return {
	    .name = "gb-irq",
	    .help = "test Game Boy CPU interrupt handling",
	    .description = "Exercises CPU interrupt dispatch, priority, IF/IE clearing and HALT wake-up without loading a ROM.",
	    .usage = "gb-irq",
	    .arguments = {},
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbIrqProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto* workspace = gameBoyWorkspace(context);
	if (workspace == nullptr) {
		return 1;
	}

	auto& bus = workspace->bus;
	auto& cpu = workspace->cpu;

	context.writeLine("gb-irq: CPU interrupt handling smoke test");

	bus.reset();
	cpu.resetToPostBootState();
	cpu.setIme(true);
	bus.write8(0xFFFF, lexxos::gameboy::interruptTimer);
	bus.write8(0xFF0F, 0x00);
	bus.requestInterrupt(lexxos::gameboy::interruptTimer);
	writeBusReadByte(context, bus, "ie-timer", 0xFFFF);
	writeBusReadByte(context, bus, "if-before-timer", 0xFF0F);
	auto step = cpu.step(bus);
	writeCpuStep(context, 0, step, cpu);
	writeBusReadByte(context, bus, "if-after-timer-service", 0xFF0F);

	bus.reset();
	cpu.resetToPostBootState();
	cpu.setIme(true);
	bus.write8(0xFFFF,
	    static_cast<std::uint8_t>(lexxos::gameboy::interruptVBlank | lexxos::gameboy::interruptTimer | lexxos::gameboy::interruptJoypad));
	bus.write8(0xFF0F, 0x00);
	bus.requestInterrupt(lexxos::gameboy::interruptTimer);
	bus.requestInterrupt(lexxos::gameboy::interruptJoypad);
	bus.requestInterrupt(lexxos::gameboy::interruptVBlank);
	writeBusReadByte(context, bus, "if-before-priority", 0xFF0F);
	step = cpu.step(bus);
	writeCpuStep(context, 1, step, cpu);
	writeBusReadByte(context, bus, "if-after-vblank-service", 0xFF0F);

	bus.reset();
	cpu.resetToPostBootState();
	cpu.setIme(false);
	cpu.setPc(0xC000);
	bus.write8(0xC000, 0x76);
	bus.write8(0xC001, 0x00);
	bus.write8(0xFFFF, lexxos::gameboy::interruptVBlank);
	bus.write8(0xFF0F, 0x00);
	step = cpu.step(bus);
	writeCpuStep(context, 2, step, cpu);
	bus.requestInterrupt(lexxos::gameboy::interruptVBlank);
	step = cpu.step(bus);
	writeCpuStep(context, 3, step, cpu);
	writeBusReadByte(context, bus, "if-after-halt-wakeup", 0xFF0F);

	context.writeLine("irq: ok");
	return 0;
}

} // namespace lexxos::apps::gameboy_programs
