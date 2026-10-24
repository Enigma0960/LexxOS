#include <lexxos/apps/gameboy/gb_bus_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbBusProgram::descriptor() {
	return {
	    .name = "gb-bus",
	    .help = "test Game Boy memory bus",
	    .description = "Opens a Game Boy ROM as a cartridge, attaches it to a 64 KiB bus model and tests ROM, VRAM, WRAM, echo RAM, OAM, IO, HRAM and IE mapping.",
	    .usage = "gb-bus [--bank number] <rom>",
	    .arguments = argumentList(gbBusArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbBusProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, arguments.positional("rom"), path)) {
		return 1;
	}

	std::uint32_t requestedBank = 2;
	if (!parseUnsigned(arguments.valueOr("bank", "2"), requestedBank)) {
		context.writeErrorLine("gb-bus: invalid bank number");
		return 2;
	}

	auto* workspace = gameBoyWorkspace(context);
	if (workspace == nullptr) {
		return 1;
	}

	auto& cartridge = workspace->cartridge;
	if (!openCartridge(context, "gb-bus", path.data(), cartridge)) {
		return 1;
	}

	// The bus contains VRAM and WRAM arrays. Reuse the shared Game Boy workspace so
	// diagnostic programs do not reserve multiple 16 KiB Bus instances in .bss.
	auto& bus = workspace->bus;
	bus.reset();
	bus.attachCartridge(&cartridge);

	auto const& info = cartridge.headerInfo();
	auto const romBanks = cartridge.expectedRomBanks();
	if (requestedBank >= romBanks && romBanks != 0) {
		requestedBank %= romBanks;
	}
	if (requestedBank == 0 && romBanks > 1) {
		requestedBank = 1;
	}

	context.write("gb-bus: loaded ");
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
	context.write("memory: vram=8192 wram=8192 oam=160 io=128 hram=127");
	context.writeLine("");

	writeBusReadByte(context, bus, "entry", 0x0100);
	writeBusReadByte(context, bus, "switchable-before", 0x4000);
	context.write("selected bank before: ");
	writeUnsigned(context, cartridge.selectedRomBank());
	context.writeLine("");

	if (cartridge.mapper() == lexxos::gameboy::CartridgeMapper::Mbc1) {
		bus.write8(0x2000, static_cast<std::uint8_t>(requestedBank & 0x1FU));
		bus.write8(0x4000, static_cast<std::uint8_t>((requestedBank >> 5U) & 0x03U));
	} else if (cartridge.mapper() == lexxos::gameboy::CartridgeMapper::Mbc5) {
		bus.write8(0x2000, static_cast<std::uint8_t>(requestedBank & 0xFFU));
		bus.write8(0x3000, static_cast<std::uint8_t>((requestedBank >> 8U) & 0x01U));
	}

	context.write("selected bank after: ");
	writeUnsigned(context, cartridge.selectedRomBank());
	context.writeLine("");
	writeBusReadByte(context, bus, "switchable-after", 0x4000);

	bus.write8(0x8000, 0x12);
	writeBusReadByte(context, bus, "vram", 0x8000);
	bus.write8(0xC000, 0x34);
	writeBusReadByte(context, bus, "wram", 0xC000);
	writeBusReadByte(context, bus, "echo", 0xE000);
	bus.write8(0xE001, 0x56);
	writeBusReadByte(context, bus, "wram-after-echo-write", 0xC001);
	bus.write8(0xFE00, 0x78);
	writeBusReadByte(context, bus, "oam", 0xFE00);
	bus.write8(0xFEA0, 0x99);
	writeBusReadByte(context, bus, "unusable", 0xFEA0);
	bus.write8(0xFF00, 0xAB);
	writeBusReadByte(context, bus, "io", 0xFF00);
	bus.write8(0xFF01, 0xAB);
	writeBusReadByte(context, bus, "io", 0xFF01);
	bus.write8(0xFF00, 0x10);
	writeBusReadByte(context, bus, "joypad-buttons", 0xFF00);
	bus.write8(0xFF80, 0xCD);
	writeBusReadByte(context, bus, "hram", 0xFF80);
	bus.write8(0xFFFF, 0x1F);
	writeBusReadByte(context, bus, "ie", 0xFFFF);

	context.writeLine("bus: ok");
	bus.attachCartridge(nullptr);
	return cartridge.mapperSupported() ? 0 : 2;
}

} // namespace lexxos::apps::gameboy_programs
