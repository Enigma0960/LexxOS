#include <lexxos/apps/gameboy/gb_io_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbIoProgram::descriptor() {
	return {
	    .name = "gb-io",
	    .help = "test Game Boy IO registers",
	    .description = "Exercises the minimal Game Boy joypad, timer, divider, LY and interrupt flag model without loading a ROM.",
	    .usage = "gb-io",
	    .arguments = {},
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbIoProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto* workspace = gameBoyWorkspace(context);
	if (workspace == nullptr) {
		return 1;
	}

	auto& bus = workspace->bus;
	bus.reset();

	context.writeLine("gb-io: minimal joypad/timer/ppu IO model");
	writeBusReadByte(context, bus, "p1-default", 0xFF00);
	bus.write8(0xFF00, 0x10);
	writeBusReadByte(context, bus, "p1-buttons-idle", 0xFF00);
	bus.write8(0xFF0F, 0x00);
	bus.setJoypadState(lexxos::gameboy::JoypadState{.a = true, .start = true});
	writeBusReadByte(context, bus, "p1-buttons-a-start", 0xFF00);
	writeBusReadByte(context, bus, "if-after-buttons", 0xFF0F);

	bus.write8(0xFF00, 0x20);
	bus.write8(0xFF0F, 0x00);
	bus.setJoypadState(lexxos::gameboy::JoypadState{.right = true, .down = true});
	writeBusReadByte(context, bus, "p1-dpad-right-down", 0xFF00);
	writeBusReadByte(context, bus, "if-after-dpad", 0xFF0F);

	bus.reset();
	writeBusReadByte(context, bus, "div-initial", 0xFF04);
	writeBusReadByte(context, bus, "ly-initial", 0xFF44);
	bus.advanceCycles(256);
	writeBusReadByte(context, bus, "div-after-256", 0xFF04);
	bus.advanceCycles(200);
	writeBusReadByte(context, bus, "ly-after-456", 0xFF44);

	bus.write8(0xFF0F, 0x00);
	bus.write8(0xFF06, 0xA7);
	bus.write8(0xFF05, 0xFE);
	bus.write8(0xFF07, 0x05);
	bus.advanceCycles(16);
	writeBusReadByte(context, bus, "tima-after-16", 0xFF05);
	bus.advanceCycles(16);
	writeBusReadByte(context, bus, "tima-after-overflow", 0xFF05);
	writeBusReadByte(context, bus, "if-after-timer", 0xFF0F);
	writeBusReadByte(context, bus, "tac", 0xFF07);
	context.writeLine("io: ok");
	return 0;
}

} // namespace lexxos::apps::gameboy_programs
