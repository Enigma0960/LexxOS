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

public:
	void append(char value);
	void append(std::string_view value);
	void appendUnsigned(std::uint32_t value);
	[[nodiscard]] std::string_view view() const;
};

} // namespace lexxos::app::detail
