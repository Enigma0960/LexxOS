#include <lexxos/app/detail/picocalc_lcd_console_scrollback.h>

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleScrollback::reset() {
	for (auto& row : _rows) {
		row.fill(blankCharacter);
	}
	_head = 0;
	_count = 0;
	_droppedRows = 0;
}

bool LcdConsoleScrollback::storeRow(char const* cells, int columns) {
	if (cells == nullptr || columns <= 0) {
		return false;
	}
	if (columns > maxScrollbackColumns) {
		columns = maxScrollbackColumns;
	}

	auto& target = _rows[_head];
	bool const droppedOldest = _count >= _rows.size();
	target.fill(blankCharacter);
	for (int x = 0; x < columns; ++x) {
		target[static_cast<std::size_t>(x)] = cells[x];
	}

	_head = (_head + 1U) % _rows.size();
	if (_count < _rows.size()) {
		++_count;
	} else {
		++_droppedRows;
	}
	return droppedOldest;
}

char LcdConsoleScrollback::cell(int line, int column, int columns) const {
	if (line < 0 || column < 0 || column >= columns || column >= maxScrollbackColumns || line >= static_cast<int>(_count)) {
		return blankCharacter;
	}

	auto const oldest = (_head + _rows.size() - _count) % _rows.size();
	auto const physical = (oldest + static_cast<std::size_t>(line)) % _rows.size();
	return _rows[physical][static_cast<std::size_t>(column)];
}

std::size_t LcdConsoleScrollback::capacity() const {
	return _rows.size();
}

std::size_t LcdConsoleScrollback::count() const {
	return _count;
}

std::uint32_t LcdConsoleScrollback::droppedRows() const {
	return _droppedRows;
}

} // namespace lexxos::app::picocalc_lcd_console::detail
