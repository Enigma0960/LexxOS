#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

namespace {

	struct NamedRgb565Palette {
		std::string_view name{};
		lexxos::gameboy::Rgb565Palette palette{};
	};

	constexpr NamedRgb565Palette namedRgb565Palettes[] = {
	    {      "default", lexxos::gameboy::defaultRgb565Palette},
	    {	     "gray", lexxos::gameboy::defaultRgb565Palette},
	    {    "grayscale", lexxos::gameboy::defaultRgb565Palette},
	    {    "dmg_green",  {{{0x9DE1, 0x8D61, 0x3306, 0x09C1}}}},
	    {  "pocket_gray",  {{{0xFFDF, 0xCE59, 0x738E, 0x18C3}}}},
	    {     "gbc_blue",  {{{0xE7DA, 0x8E0E, 0x334A, 0x08C4}}}},
	    {    "gbc_brown",  {{{0xFF59, 0xDD4C, 0xA307, 0x40C1}}}},
	    {      "gbc_red",  {{{0xFF5B, 0xF510, 0xBA8A, 0x3883}}}},
	    {   "gbc_pastel",  {{{0xFF9B, 0xBED4, 0x7D56, 0x31CB}}}},
	    {"high_contrast",  {{{0xFFFF, 0xAD55, 0x52AA, 0x0000}}}},
	};

} // namespace

void writeHex16(lexxos::os::AppContext& context, std::uint16_t value) {
	context.write("0x");
	writeHexByte(context, static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
	writeHexByte(context, static_cast<std::uint8_t>(value & 0xFFU));
}

void writeCode8(lexxos::os::AppContext& context, std::uint8_t value) {
	context.write("0x");
	writeHexByte(context, value);
}

void writeNamedCode(lexxos::os::AppContext& context, std::string_view name, std::uint8_t code) {
	context.write(name);
	context.write(" (");
	writeCode8(context, code);
	context.writeLine(")");
}

void writeBytesField(lexxos::os::AppContext& context, std::uint64_t bytes) {
	writeUnsigned64(context, bytes);
	context.write(" bytes");
}

bool parseUnsigned(std::string_view value, std::uint32_t& output) {
	if (value.empty()) {
		return false;
	}

	std::uint32_t result = 0;
	for (char const character : value) {
		if (character < '0' || character > '9') {
			return false;
		}
		auto const digit = static_cast<std::uint32_t>(character - '0');
		if (result > (0xFFFFFFFFU - digit) / 10U) {
			return false;
		}
		result = result * 10U + digit;
	}

	output = result;
	return true;
}

bool parseNumber(std::string_view value, std::uint32_t& output) {
	if (value.empty()) {
		return false;
	}

	int base = 10;
	std::size_t offset = 0;
	if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
		base = 16;
		offset = 2;
	}
	if (offset == value.size()) {
		return false;
	}

	std::uint32_t result = 0;
	for (std::size_t index = offset; index < value.size(); ++index) {
		char const character = value[index];
		std::uint32_t digit = 0;
		if (character >= '0' && character <= '9') {
			digit = static_cast<std::uint32_t>(character - '0');
		} else if (base == 16 && character >= 'a' && character <= 'f') {
			digit = static_cast<std::uint32_t>(character - 'a' + 10);
		} else if (base == 16 && character >= 'A' && character <= 'F') {
			digit = static_cast<std::uint32_t>(character - 'A' + 10);
		} else {
			return false;
		}

		if (digit >= static_cast<std::uint32_t>(base)) {
			return false;
		}
		if (result > (0xFFFFFFFFU - digit) / static_cast<std::uint32_t>(base)) {
			return false;
		}
		result = result * static_cast<std::uint32_t>(base) + digit;
	}

	output = result;
	return true;
}

bool parseRgb565Palette(std::string_view value, lexxos::gameboy::Rgb565Palette& palette) {
	for (auto const& candidate : namedRgb565Palettes) {
		if (candidate.name == value || (value.empty() && candidate.name == std::string_view{"default"})) {
			palette = candidate.palette;
			return true;
		}
	}
	return false;
}

std::string_view rgb565PaletteNames() {
	return "default, dmg_green, pocket_gray, gbc_blue, gbc_brown, gbc_red, gbc_pastel or high_contrast";
}

void writeCartridgeFeature(lexxos::os::AppContext& context, std::string_view name, bool enabled) {
	context.write(name);
	context.write(":");
	context.write(enabled ? "1" : "0");
}

void writeCartridgeFeatures(lexxos::os::AppContext& context, lexxos::gameboy::CartridgeFeatures const& features) {
	context.write("features: ");
	writeCartridgeFeature(context, "ram", features.externalRam);
	context.write(" ");
	writeCartridgeFeature(context, "battery", features.battery);
	context.write(" ");
	writeCartridgeFeature(context, "timer", features.timer);
	context.write(" ");
	writeCartridgeFeature(context, "rumble", features.rumble);
	context.writeLine("");
}

void writeReadByte(lexxos::os::AppContext& context, std::string_view label, std::uint8_t value) {
	context.write(label);
	context.write(": ");
	writeCode8(context, value);
	context.writeLine("");
}

void writeBusReadByte(lexxos::os::AppContext& context, lexxos::gameboy::Bus& bus, std::string_view label, std::uint16_t address) {
	std::uint8_t value = 0;
	auto const status = bus.read8(address, value);
	context.write(label);
	context.write("[");
	writeHex16(context, address);
	context.write("]: ");
	if (status != lexxos::vfs::ReadStatus::Ok) {
		context.writeLine(lexxos::vfs::toString(status));
		return;
	}
	writeCode8(context, value);
	context.writeLine("");
}

bool openCartridge(
    lexxos::os::AppContext& context,
    std::string_view programName,
    std::string_view path,
    lexxos::gameboy::Cartridge& cartridge) {
	cartridge.bindPlatformServices(context.platform());
	auto const result = cartridge.open(path);
	if (result.ok) {
		return true;
	}

	context.write(programName);
	context.write(": ");
	context.write(path);
	context.write(": ");
	if (result.openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.writeLine(lexxos::vfs::toString(result.openStatus));
		return false;
	}
	if (result.tooSmall) {
		context.writeLine("file is too small for a Game Boy cartridge header");
		return false;
	}
	if (result.readStatus != lexxos::vfs::ReadStatus::Ok) {
		context.writeLine(lexxos::vfs::toString(result.readStatus));
		return false;
	}
	if (result.invalidHeader) {
		context.writeLine("cartridge header validation failed");
		return false;
	}

	context.writeLine("failed to open cartridge");
	return false;
}

} // namespace lexxos::apps::gameboy_programs
