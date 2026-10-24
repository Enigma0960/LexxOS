#include <lexxos/apps/gameboy/gb_cart_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbCartProgram::descriptor() {
	return {
	    .name = "gb-cart",
	    .help = "test Game Boy cartridge mapper",
	    .description = "Opens a Game Boy ROM as a cartridge backend and performs small ROM bank mapping reads without loading the full image.",
	    .usage = "gb-cart [--bank number] <rom>",
	    .arguments = argumentList(gbCartArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbCartProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, arguments.positional("rom"), path)) {
		return 1;
	}

	std::uint32_t requestedBank = 2;
	if (!parseUnsigned(arguments.valueOr("bank", "2"), requestedBank)) {
		context.writeErrorLine("gb-cart: invalid bank number");
		return 2;
	}

	auto* workspace = gameBoyWorkspace(context);
	if (workspace == nullptr) {
		return 1;
	}

	auto& cartridge = workspace->cartridge;
	if (!openCartridge(context, "gb-cart", path.data(), cartridge)) {
		return 1;
	}

	CartridgeLifetimeGuard cartridgeGuard{cartridge};

	auto const& info = cartridge.headerInfo();
	auto const expectedRomSize = cartridge.expectedRomSize();
	auto const expectedRamSize = cartridge.expectedRamSize();
	auto const romBanks = cartridge.expectedRomBanks();
	auto const ramBanks = cartridge.expectedRamBanks();

	context.write("gb-cart: loaded ");
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
	context.write("mapper supported: ");
	context.writeLine(cartridge.mapperSupported() ? "yes" : "no");
	writeCartridgeFeatures(context, cartridge.features());

	context.write("rom banks: ");
	writeUnsigned(context, romBanks);
	context.write(" (");
	writeBytesField(context, expectedRomSize);
	context.writeLine(")");
	context.write("ram banks: ");
	writeUnsigned(context, ramBanks);
	context.write(" (");
	writeBytesField(context, expectedRamSize);
	context.writeLine(")");

	std::uint8_t value = 0;
	auto status = cartridge.read8(0x0100, value);
	if (status != lexxos::vfs::ReadStatus::Ok) {
		context.writeError("gb-cart: failed to read bank0[0100]: ");
		context.writeErrorLine(lexxos::vfs::toString(status));
		return 1;
	}
	writeReadByte(context, "bank0[0100]", value);

	status = cartridge.read8(0x4000, value);
	if (status != lexxos::vfs::ReadStatus::Ok) {
		context.writeError("gb-cart: failed to read selected[4000]: ");
		context.writeErrorLine(lexxos::vfs::toString(status));
		return 1;
	}
	context.write("selected bank: ");
	writeUnsigned(context, cartridge.selectedRomBank());
	context.writeLine("");
	writeReadByte(context, "selected[4000]", value);

	if (requestedBank >= romBanks && romBanks != 0) {
		requestedBank %= romBanks;
	}
	if (requestedBank == 0 && romBanks > 1) {
		requestedBank = 1;
	}

	if (cartridge.mapper() == lexxos::gameboy::CartridgeMapper::Mbc1) {
		cartridge.write8(0x2000, static_cast<std::uint8_t>(requestedBank & 0x1FU));
		cartridge.write8(0x4000, static_cast<std::uint8_t>((requestedBank >> 5U) & 0x03U));
	} else if (cartridge.mapper() == lexxos::gameboy::CartridgeMapper::Mbc2) {
		cartridge.write8(0x2100, static_cast<std::uint8_t>(requestedBank & 0x0FU));
	} else if (cartridge.mapper() == lexxos::gameboy::CartridgeMapper::Mbc5) {
		cartridge.write8(0x2000, static_cast<std::uint8_t>(requestedBank & 0xFFU));
		cartridge.write8(0x3000, static_cast<std::uint8_t>((requestedBank >> 8U) & 0x01U));
	}

	status = cartridge.read8(0x4000, value);
	if (status != lexxos::vfs::ReadStatus::Ok) {
		context.writeError("gb-cart: failed to read after-switch[4000]: ");
		context.writeErrorLine(lexxos::vfs::toString(status));
		return 1;
	}
	context.write("after switch bank: ");
	writeUnsigned(context, cartridge.selectedRomBank());
	context.writeLine("");
	writeReadByte(context, "after-switch[4000]", value);
	context.writeLine("cache: 512-byte ROM read cache");

	return cartridge.mapperSupported() ? 0 : 2;
}

} // namespace lexxos::apps::gameboy_programs
