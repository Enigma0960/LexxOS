#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleScrollback final {
public:
	void reset();

	[[nodiscard]] bool storeRow(char const* cells, int columns);

	[[nodiscard]] char cell(int line, int column, int columns) const;
	[[nodiscard]] std::size_t capacity() const;
	[[nodiscard]] std::size_t count() const;
	[[nodiscard]] std::uint32_t droppedRows() const;

private:
	std::array<std::array<char, maxScrollbackColumns>, scrollbackRows> _rows{};
	std::size_t _head{0};
	std::size_t _count{0};
	std::uint32_t _droppedRows{0};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
