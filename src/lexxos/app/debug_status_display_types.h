#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::app::detail {

struct DisplayLine {
	static constexpr std::size_t capacity = 21;

	std::array<char, capacity> text{};
	std::size_t size{0};

	void append(char value) {
		if (size >= text.size()) {
			return;
		}
		text[size++] = value;
	}

	void append(std::string_view value) {
		for (char const character : value) {
			append(character);
		}
	}

	void appendUnsigned(std::uint32_t value) {
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

	[[nodiscard]] std::string_view view() const {
		return std::string_view{text.data(), size};
	}
};

} // namespace lexxos::app::detail
