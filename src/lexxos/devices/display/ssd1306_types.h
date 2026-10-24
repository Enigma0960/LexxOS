#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lexxos::devices::display::detail {

struct Glyph {
	static constexpr std::size_t columnCount = 5;

	char value{' '};
	std::array<std::uint8_t, columnCount> columns{};
};

} // namespace lexxos::devices::display::detail
