#include <lexxos/apps/gameboy/gb_info_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbInfoProgram::descriptor() {
	return {
	    .name = "gb-info",
	    .help = "show Game Boy ROM cartridge header",
	    .description = "Reads the first 0x150 bytes of a Game Boy ROM image through VFS and prints cartridge metadata.",
	    .usage = "gb-info <rom>",
	    .arguments = argumentList(romPathArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbInfoProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, arguments.positional("rom"), path)) {
		return 1;
	}

	std::array<std::uint8_t, lexxos::gameboy::cartridgeHeaderSize> header{};
	lexxos::vfs::FileHandle file;
	if (!readCartridgeHeader(context, path.data(), header, file)) {
		return 1;
	}

	auto const info = lexxos::gameboy::parseCartridgeHeader(header.data(), header.size());
	auto const expectedRomSize = lexxos::gameboy::romSizeBytes(info.romSizeCode);
	auto const expectedRamSize = lexxos::gameboy::ramSizeBytes(info.ramSizeCode);

	context.write("path: ");
	context.writeLine(path.data());
	context.write("file size: ");
	writeBytesField(context, file.size());
	context.writeLine("");
	context.write("title: ");
	context.writeLine(info.title.data()[0] != '\0' ? info.title.data() : "<empty>");

	context.write("cartridge type: ");
	writeNamedCode(context, lexxos::gameboy::cartridgeTypeName(info.cartridgeType), info.cartridgeType);

	context.write("rom size: ");
	context.write(lexxos::gameboy::romSizeName(info.romSizeCode));
	context.write(" (");
	writeCode8(context, info.romSizeCode);
	if (expectedRomSize != 0) {
		context.write(", ");
		writeBytesField(context, expectedRomSize);
	}
	context.writeLine(")");

	context.write("ram size: ");
	context.write(lexxos::gameboy::ramSizeName(info.ramSizeCode));
	context.write(" (");
	writeCode8(context, info.ramSizeCode);
	context.write(", ");
	writeBytesField(context, expectedRamSize);
	context.writeLine(")");

	context.write("destination: ");
	writeNamedCode(context, lexxos::gameboy::destinationName(info.destinationCode), info.destinationCode);
	context.write("cgb: ");
	writeNamedCode(context, lexxos::gameboy::cgbFlagName(info.cgbFlag), info.cgbFlag);
	context.write("sgb: ");
	writeNamedCode(context, lexxos::gameboy::sgbFlagName(info.sgbFlag), info.sgbFlag);
	context.write("old licensee: ");
	writeCode8(context, info.oldLicenseeCode);
	context.writeLine("");
	context.write("mask rom version: ");
	writeUnsigned(context, info.maskRomVersion);
	context.writeLine("");
	context.write("nintendo logo: ");
	context.writeLine(info.nintendoLogoOk ? "ok" : "error");
	context.write("header checksum: ");
	context.write(info.headerChecksumOk ? "ok" : "error");
	context.write(" (stored=");
	writeCode8(context, info.headerChecksum);
	context.write(", calculated=");
	writeCode8(context, info.calculatedHeaderChecksum);
	context.writeLine(")");
	context.write("global checksum: ");
	writeHex16(context, info.globalChecksum);
	context.writeLine(" (stored, not verified)");
	writeRomSizeConsistency(context, file.size(), expectedRomSize);

	return info.headerChecksumOk && info.nintendoLogoOk ? 0 : 2;
}

} // namespace lexxos::apps::gameboy_programs
