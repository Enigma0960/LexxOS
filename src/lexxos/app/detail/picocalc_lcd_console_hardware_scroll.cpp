#include <lexxos/app/detail/picocalc_lcd_console_hardware_scroll.h>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleHardwareScroll::resetState(int rows) {
	_enabled = false;
	_areaPixels = rows * cellHeight;
	_startPixels = 0;
}

void LcdConsoleHardwareScroll::configure(lexxos::devices::DisplayDevice* display, int rows) {
	resetState(rows);
	if (display == nullptr || _areaPixels <= 0 || statusBarHeightPixels >= display->height() || _areaPixels > display->height() - statusBarHeightPixels) {
		return;
	}

	auto const bottomFixedPixels = display->height() - statusBarHeightPixels - _areaPixels;
	_enabled = display->configureVerticalScroll(statusBarHeightPixels, _areaPixels, bottomFixedPixels)
	    && display->setVerticalScrollStart(0);
}

void LcdConsoleHardwareScroll::reset(lexxos::devices::DisplayDevice* display, bool resetScroll) {
	if (!resetScroll || !_enabled || display == nullptr) {
		return;
	}
	_startPixels = 0;
	if (!display->setVerticalScrollStart(0)) {
		_enabled = false;
	}
}

bool LcdConsoleHardwareScroll::advance(lexxos::devices::DisplayDevice* display, int pixels) {
	if (!_enabled || display == nullptr || _areaPixels <= 0) {
		return false;
	}
	_startPixels = (_startPixels + pixels) % _areaPixels;
	if (display->setVerticalScrollStart(_startPixels)) {
		return true;
	}
	_enabled = false;
	_startPixels = 0;
	return false;
}

int LcdConsoleHardwareScroll::rowTopPixel(int row) const {
	if (!_enabled || _areaPixels <= 0) {
		return statusBarHeightPixels + row * cellHeight;
	}

	return statusBarHeightPixels + ((_startPixels + row * cellHeight) % _areaPixels);
}

bool LcdConsoleHardwareScroll::enabled() const {
	return _enabled;
}

int LcdConsoleHardwareScroll::areaPixels() const {
	return _areaPixels;
}

int LcdConsoleHardwareScroll::startPixels() const {
	return _startPixels;
}

} // namespace lexxos::app::picocalc_lcd_console::detail
