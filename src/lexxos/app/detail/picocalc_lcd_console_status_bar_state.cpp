#include <lexxos/app/detail/picocalc_lcd_console_status_bar_state.h>

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleStatusBarState::set(lexxos::app::picocalc_lcd_console::StatusBarInfo const& info) {
	auto const normalized = _normalize(info);
	_dirty = _dirty || _changed(normalized);
	_status = normalized;
}

void LcdConsoleStatusBarState::markDirty() {
	_dirty = true;
}

bool LcdConsoleStatusBarState::takeDirty() {
	bool const dirty = _dirty;
	_dirty = false;
	return dirty;
}

lexxos::app::picocalc_lcd_console::StatusBarInfo LcdConsoleStatusBarState::snapshot() const {
	return _status;
}

lexxos::app::picocalc_lcd_console::StatusBarInfo LcdConsoleStatusBarState::_normalize(
    lexxos::app::picocalc_lcd_console::StatusBarInfo info) {
	if (info.batteryPercent > 100) {
		info.batteryPercent = 100;
	}

	if (info.hour > 23) {
		info.timeValid = false;
		info.hour = 0;
	}

	if (info.minute > 59) {
		info.timeValid = false;
		info.minute = 0;
	}

	if (info.second > 59) {
		info.timeValid = false;
		info.second = 0;
	}

	if (info.wifiSignalLevel > 3) {
		info.wifiSignalLevel = 3;
	}

	if (!info.wifiValid || !info.wifiConnected) {
		info.wifiConnected = false;
		info.wifiSignalLevel = 0;
	} else if (info.wifiSignalLevel == 0) {
		info.wifiSignalLevel = 1;
	}

	return info;
}

bool LcdConsoleStatusBarState::_changed(
    lexxos::app::picocalc_lcd_console::StatusBarInfo const& normalized) const {
	return _status.visible != normalized.visible
	    || _status.batteryValid != normalized.batteryValid
	    || _status.batteryPercent != normalized.batteryPercent
	    || _status.batteryCharging != normalized.batteryCharging
	    || _status.timeValid != normalized.timeValid
	    || _status.hour != normalized.hour
	    || _status.minute != normalized.minute
	    || _status.second != normalized.second
	    || _status.wifiValid != normalized.wifiValid
	    || _status.wifiConnected != normalized.wifiConnected
	    || _status.wifiSignalLevel != normalized.wifiSignalLevel;
}

} // namespace lexxos::app::picocalc_lcd_console::detail
