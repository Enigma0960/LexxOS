#include <lexxos/apps/devices/detail/device_program_support.h>

namespace lexxos::apps::device_programs::detail {

bool parseUnsigned(std::string_view text, std::uint32_t& output) {
	if (text.empty()) {
		return false;
	}

	std::uint32_t base = 10;
	std::size_t index = 0;
	if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
		base = 16;
		index = 2;
	}
	if (index >= text.size()) {
		return false;
	}

	std::uint32_t value = 0;
	for (; index < text.size(); ++index) {
		char const character = text[index];
		std::uint32_t digit = 0;
		if (character >= '0' && character <= '9') {
			digit = static_cast<std::uint32_t>(character - '0');
		} else if (character >= 'a' && character <= 'f') {
			digit = static_cast<std::uint32_t>(10 + character - 'a');
		} else if (character >= 'A' && character <= 'F') {
			digit = static_cast<std::uint32_t>(10 + character - 'A');
		} else {
			return false;
		}

		if (digit >= base || value > (0xFFFFFFFFU - digit) / base) {
			return false;
		}
		value = value * base + digit;
	}

	output = value;
	return true;
}

} // namespace lexxos::apps::device_programs::detail
