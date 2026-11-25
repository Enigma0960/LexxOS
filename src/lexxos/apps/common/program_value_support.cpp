#include <limits>

#include <lexxos/apps/common/core_programs_support.h>

namespace lexxos::apps {

void writeUnsigned(lexxos::os::AppContext& shell, std::uint32_t value) {
	char buffer[16]{};
	int position = static_cast<int>(sizeof(buffer)) - 1;
	buffer[position] = '\0';

	if (value == 0) {
		shell.write("0");
		return;
	}

	while (value != 0 && position > 0) {
		--position;
		buffer[position] = static_cast<char>('0' + (value % 10));
		value /= 10;
	}

	shell.write(&buffer[position]);
}

void writeUnsigned64(lexxos::os::AppContext& shell, std::uint64_t value) {
	char buffer[32]{};
	int position = static_cast<int>(sizeof(buffer)) - 1;
	buffer[position] = '\0';

	if (value == 0) {
		shell.write("0");
		return;
	}

	while (value != 0 && position > 0) {
		--position;
		buffer[position] = static_cast<char>('0' + (value % 10));
		value /= 10;
	}

	shell.write(&buffer[position]);
}

char hexDigit(std::uint8_t value) {
	return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
}

void writeHex8(lexxos::os::AppContext& shell, std::uint8_t value) {
	char buffer[5]{
	    '0',
	    'x',
	    hexDigit(static_cast<std::uint8_t>((value >> 4) & 0x0FU)),
	    hexDigit(static_cast<std::uint8_t>(value & 0x0FU)),
	    '\0',
	};
	shell.write(buffer);
}

void writeHexByte(lexxos::os::AppContext& shell, std::uint8_t value) {
	char buffer[3]{
	    hexDigit(static_cast<std::uint8_t>((value >> 4) & 0x0FU)),
	    hexDigit(static_cast<std::uint8_t>(value & 0x0FU)),
	    '\0',
	};
	shell.write(buffer);
}

void writeHex32(lexxos::os::AppContext& shell, std::uint32_t value) {
	for (int shift = 28; shift >= 0; shift -= 4) {
		std::uint8_t const digit = static_cast<std::uint8_t>((value >> shift) & 0x0FU);
		char const character = hexDigit(digit);
		shell.write(std::string_view{&character, 1});
	}
}

bool parseUnsigned64(std::string_view text, std::uint64_t& value) {
	if (text.empty()) {
		return false;
	}

	std::uint64_t result = 0;
	for (char const character : text) {
		if (character < '0' || character > '9') {
			return false;
		}

		std::uint64_t const digit = static_cast<std::uint64_t>(character - '0');
		if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10ULL) {
			return false;
		}

		result = result * 10ULL + digit;
	}

	value = result;
	return true;
}

bool parseUnsigned32(std::string_view text, std::uint32_t& value) {
	std::uint64_t parsed = 0;
	if (!parseUnsigned64(text, parsed) || parsed > std::numeric_limits<std::uint32_t>::max()) {
		return false;
	}

	value = static_cast<std::uint32_t>(parsed);
	return true;
}

bool isPrintableAscii(std::uint8_t value) {
	return value >= 0x20U && value <= 0x7EU;
}

std::uint16_t readLe16(std::uint8_t const* data) {
	return static_cast<std::uint16_t>(data[0]) | static_cast<std::uint16_t>(data[1] << 8);
}

std::uint32_t readLe32(std::uint8_t const* data) {
	return static_cast<std::uint32_t>(data[0])
	    | (static_cast<std::uint32_t>(data[1]) << 8)
	    | (static_cast<std::uint32_t>(data[2]) << 16)
	    | (static_cast<std::uint32_t>(data[3]) << 24);
}

bool resolvePath(lexxos::os::AppContext& context, std::string_view path, std::array<char, lexxos::vfs::maxPathLength>& output) {
	if (!lexxos::vfs::normalizePath(context.workingDirectory(), path, output.data(), output.size())) {
		context.writeLine("invalid path");
		return false;
	}
	return true;
}

} // namespace lexxos::apps
