#include <lexxos/app/debug_status_display_types.h>

namespace lexxos::app::detail {

void DisplayLine::append(char value) {
	if (size >= text.size()) {
		return;
	}
	text[size++] = value;
}

void DisplayLine::append(std::string_view value) {
	for (char const character : value) {
		append(character);
	}
}

void DisplayLine::appendUnsigned(std::uint32_t value) {
	std::array<char, 10> digits{};
	std::size_t digitCount = 0;

	if (value == 0) {
		append('0');
		return;
	}

	while (value != 0 && digitCount < digits.size()) {
		digits[digitCount++] = static_cast<char>('0' + value % 10);
		value /= 10;
	}

	while (digitCount > 0) {
		append(digits[--digitCount]);
	}
}

[[nodiscard]] std::string_view DisplayLine::view() const {
	return std::string_view{text.data(), size};
}

} // namespace lexxos::app::detail
