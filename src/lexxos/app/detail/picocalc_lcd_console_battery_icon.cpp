#include <lexxos/app/detail/picocalc_lcd_console_battery_icon.h>

namespace lexxos::app::picocalc_lcd_console::detail {

int BatteryIcon::width() const {
	return statusBarBatteryIconWidth;
}

void BatteryIcon::draw(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status,
    int x,
    int rowY,
    std::uint16_t* pixels,
    int pixelWidth) const {
	auto const bodyX = x;
	auto const bodyY = statusBarBatteryY;
	if (bodyX < 0 || rowY < bodyY || rowY >= bodyY + statusBarBatteryHeight) {
		return;
	}

	auto const outlineColor = _outlineColor(status);
	bool const borderRow = rowY == bodyY || rowY == bodyY + statusBarBatteryHeight - 1;
	if (borderRow) {
		_fillRange(pixels, pixelWidth, bodyX, bodyX + statusBarBatteryWidth - 1, outlineColor);
	} else {
		_setPixel(pixels, pixelWidth, bodyX, outlineColor);
		_setPixel(pixels, pixelWidth, bodyX + statusBarBatteryWidth - 1, outlineColor);
	}

	auto const terminalY = bodyY + 3;
	auto const terminalHeight = statusBarBatteryHeight - 6;
	if (rowY >= terminalY && rowY < terminalY + terminalHeight) {
		_fillRange(pixels,
		    pixelWidth,
		    bodyX + statusBarBatteryWidth,
		    bodyX + statusBarBatteryWidth + statusBarBatteryTerminalWidth - 1,
		    outlineColor);
	}

	auto const percent = _batteryPercent(status);
	if (percent > 0 && !borderRow) {
		auto const fillWidth = ((statusBarBatteryWidth - 4) * percent + 99) / 100;
		if (fillWidth > 0) {
			_fillRange(pixels, pixelWidth, bodyX + 2, bodyX + 1 + fillWidth, _fillColor(percent));
		}
	}

	if (status.batteryCharging) {
		_drawChargingPlus(bodyX, bodyY, rowY, pixels, pixelWidth);
	}
}

std::uint8_t BatteryIcon::_batteryPercent(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const {
	if (!status.batteryValid) {
		return 0;
	}
	return status.batteryPercent > 100 ? 100 : status.batteryPercent;
}

std::uint16_t BatteryIcon::_outlineColor(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const {
	if (!status.batteryValid) {
		return statusSeparatorColor;
	}
	return status.batteryCharging ? statusAccentColor : statusTextColor;
}

std::uint16_t BatteryIcon::_fillColor(std::uint8_t percent) const {
	if (percent <= lowBatteryPercent) {
		return statusLowBatteryColor;
	}
	if (percent <= mediumBatteryPercent) {
		return statusMediumBatteryColor;
	}
	return statusAccentColor;
}

void BatteryIcon::_drawChargingPlus(int bodyX, int bodyY, int rowY, std::uint16_t* pixels, int pixelWidth) const {
	constexpr int halfLength = 2;
	auto const centerX = bodyX + statusBarBatteryWidth / 2;
	auto const centerY = bodyY + statusBarBatteryHeight / 2;
	if (rowY == centerY) {
		_fillRange(pixels, pixelWidth, centerX - halfLength, centerX + halfLength, statusTextColor);
	}
	if (rowY >= centerY - halfLength && rowY <= centerY + halfLength) {
		_setPixel(pixels, pixelWidth, centerX, statusTextColor);
	}
}

} // namespace lexxos::app::picocalc_lcd_console::detail
