#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleScreenBuffer {
public:
	void clearCells();

	void clearDirtyCells();

	[[nodiscard]] bool hasDirtyCells(int columns, int rows) const;

	[[nodiscard]] bool markDirtyCell(int column, int row, int columns, int rows);

	void clearDirtyCellsInRow(int row, int columns, int rows);

	void scrollDirtyCellsUp(int columns, int rows);

	char& cell(int column, int row);

	[[nodiscard]] char cell(int column, int row) const;

	std::uint8_t& attribute(int column, int row);

	[[nodiscard]] std::uint8_t attribute(int column, int row) const;

	bool& dirtyCell(int column, int row);

	[[nodiscard]] bool dirtyCell(int column, int row) const;

private:
	[[nodiscard]] static std::size_t _cellIndex(int column, int row);

	std::array<char, maxColumns * maxRows> _cells{};
	std::array<std::uint8_t, maxColumns * maxRows> _attributes{};
	std::array<bool, maxColumns * maxRows> _dirtyCells{};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
