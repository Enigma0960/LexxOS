#include <lexxos/app/detail/picocalc_lcd_console_layout.h>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>
#include <lexxos/devices/display_device.h>

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleLayout::reset() {
	_columns = 0;
	_visibleColumns = 0;
	_rows = 0;
}

bool LcdConsoleLayout::configure(lexxos::devices::DisplayDevice const& display) {
	reset();

	_visibleColumns = display.width() / cellWidth;
	if (_visibleColumns > maxVisibleColumns) {
		_visibleColumns = maxVisibleColumns;
	}
	_columns = maxColumns;

	auto const consoleHeightPixels = display.height() > statusBarHeightPixels ? display.height() - statusBarHeightPixels : 0;
	_rows = consoleHeightPixels / cellHeight;
	if (_rows > maxRows) {
		_rows = maxRows;
	}

	if (!valid()) {
		reset();
		return false;
	}
	return true;
}

bool LcdConsoleLayout::valid() const {
	return _visibleColumns > 0 && _columns > 0 && _rows > 0;
}

int LcdConsoleLayout::columns() const {
	return _columns;
}

int LcdConsoleLayout::visibleColumns() const {
	return _visibleColumns;
}

int LcdConsoleLayout::rows() const {
	return _rows;
}

int LcdConsoleLayout::maxViewLeftColumn() const {
	return _columns > _visibleColumns ? _columns - _visibleColumns : 0;
}

} // namespace lexxos::app::picocalc_lcd_console::detail
