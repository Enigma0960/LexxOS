#include <lexxos/app/detail/picocalc_lcd_console_screen_buffer.h>

#include <algorithm>

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleScreenBuffer::clearCells() {
	_firstRow = 0;
	_rowCount = maxRows;
	for (auto& cellValue : _cells) {
		cellValue = blankCharacter;
	}

	for (auto& attributeValue : _attributes) {
		attributeValue = defaultCellAttribute;
	}
}

void LcdConsoleScreenBuffer::clearDirtyCells() {
	for (auto& dirty : _dirtyCells) {
		dirty = false;
	}
}

bool LcdConsoleScreenBuffer::hasDirtyCells(int columns, int rows) const {
	for (int y = 0; y < rows; ++y) {
		for (int x = 0; x < columns; ++x) {
			if (dirtyCell(x, y)) {
				return true;
			}
		}
	}

	return false;
}

bool LcdConsoleScreenBuffer::markDirtyCell(int column, int row, int columns, int rows) {
	if (column < 0 || row < 0 || column >= columns || row >= rows) {
		return false;
	}
	dirtyCell(column, row) = true;
	return true;
}

void LcdConsoleScreenBuffer::clearDirtyCellsInRow(int row, int columns, int rows) {
	if (row < 0 || row >= rows) {
		return;
	}

	for (int x = 0; x < columns; ++x) {
		dirtyCell(x, row) = false;
	}
}

void LcdConsoleScreenBuffer::scrollDirtyCellsUp(int columns, int rows) {
	if (columns <= 0 || rows <= 0) {
		return;
	}

	for (int y = 1; y < rows; ++y) {
		for (int x = 0; x < columns; ++x) {
			dirtyCell(x, y - 1) = dirtyCell(x, y);
		}
	}

	for (int x = 0; x < columns; ++x) {
		dirtyCell(x, rows - 1) = false;
	}
}

void LcdConsoleScreenBuffer::advanceLine(int columns, int rows) {
	if (columns <= 0 || columns > maxColumns || rows <= 0 || rows > maxRows) {
		return;
	}

	// Layout changes require clearCells(), as they do for the rest of the terminal.
	_rowCount = rows;
	if (++_firstRow == rows) {
		_firstRow = 0;
	}

	auto const offset = _physicalIndex(0, rows - 1);
	std::fill_n(_cells.data() + offset, columns, blankCharacter);
	std::fill_n(_attributes.data() + offset, columns, defaultCellAttribute);
}

std::size_t LcdConsoleScreenBuffer::_physicalIndex(int column, int row) const {
	auto physicalRow = _firstRow + row;
	if (physicalRow >= _rowCount) {
		physicalRow -= _rowCount;
	}

	return static_cast<std::size_t>(physicalRow * maxColumns + column);
}

char& LcdConsoleScreenBuffer::cell(int column, int row) {
	return _cells[_physicalIndex(column, row)];
}

char LcdConsoleScreenBuffer::cell(int column, int row) const {
	return _cells[_physicalIndex(column, row)];
}

std::uint8_t& LcdConsoleScreenBuffer::attribute(int column, int row) {
	return _attributes[_physicalIndex(column, row)];
}

std::uint8_t LcdConsoleScreenBuffer::attribute(int column, int row) const {
	return _attributes[_physicalIndex(column, row)];
}

bool& LcdConsoleScreenBuffer::dirtyCell(int column, int row) {
	return _dirtyCells[_cellIndex(column, row)];
}

bool LcdConsoleScreenBuffer::dirtyCell(int column, int row) const {
	return _dirtyCells[_cellIndex(column, row)];
}

std::size_t LcdConsoleScreenBuffer::_cellIndex(int column, int row) {
	return static_cast<std::size_t>(row * maxColumns + column);
}

} // namespace lexxos::app::picocalc_lcd_console::detail
