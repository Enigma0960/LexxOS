#include <lexxos/app/detail/picocalc_lcd_console_wifi_icon.h>

namespace lexxos::app::picocalc_lcd_console::detail {
namespace {

	std::uint8_t normalizedLevel(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) {
		if (!status.wifiValid || !status.wifiConnected) {
			return 0;
		}
		if (status.wifiSignalLevel == 0) {
			return 1;
		}
		return status.wifiSignalLevel > 3 ? 3 : status.wifiSignalLevel;
	}

	std::uint16_t iconColor(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) {
		return status.wifiValid && status.wifiConnected ? statusTextColor : statusSeparatorColor;
	}

} // namespace

int WifiIcon::width() const {
	return statusBarWifiIconWidth;
}

void WifiIcon::draw(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status,
    int x,
    int rowY,
    std::uint16_t* pixels,
    int pixelWidth) const {
	if (!status.wifiValid) {
		return;
	}

	auto const sourceY = rowY - statusBarWifiIconY;
	if (sourceY < 0 || sourceY >= statusBarWifiIconHeight) {
		return;
	}

	auto const level = normalizedLevel(status);
	auto const color = iconColor(status);
	_drawDot(x, sourceY, pixels, pixelWidth, color);
	if (level >= 1) {
		_drawLowArc(x, sourceY, pixels, pixelWidth, color);
	}
	if (level >= 2) {
		_drawMidArc(x, sourceY, pixels, pixelWidth, color);
	}
	if (level >= 3) {
		_drawHighArc(x, sourceY, pixels, pixelWidth, color);
	}
}

void WifiIcon::_drawHighArc(int x, int sourceY, std::uint16_t* pixels, int pixelWidth, std::uint16_t color) const {
	switch (sourceY) {
		case 1:
			_fillRange(pixels, pixelWidth, x + 5, x + 12, color);
			break;
		case 2:
			_fillRange(pixels, pixelWidth, x + 3, x + 5, color);
			_fillRange(pixels, pixelWidth, x + 12, x + 14, color);
			break;
		case 3:
			_fillRange(pixels, pixelWidth, x + 1, x + 3, color);
			_fillRange(pixels, pixelWidth, x + 14, x + 16, color);
			break;
		case 4:
			_setPixel(pixels, pixelWidth, x, color);
			_setPixel(pixels, pixelWidth, x + 17, color);
			break;
		default:
			break;
	}
}

void WifiIcon::_drawMidArc(int x, int sourceY, std::uint16_t* pixels, int pixelWidth, std::uint16_t color) const {
	switch (sourceY) {
		case 5:
			_fillRange(pixels, pixelWidth, x + 6, x + 11, color);
			break;
		case 6:
			_fillRange(pixels, pixelWidth, x + 4, x + 6, color);
			_fillRange(pixels, pixelWidth, x + 11, x + 13, color);
			break;
		case 7:
			_fillRange(pixels, pixelWidth, x + 3, x + 4, color);
			_fillRange(pixels, pixelWidth, x + 13, x + 14, color);
			break;
		default:
			break;
	}
}

void WifiIcon::_drawLowArc(int x, int sourceY, std::uint16_t* pixels, int pixelWidth, std::uint16_t color) const {
	switch (sourceY) {
		case 9:
			_fillRange(pixels, pixelWidth, x + 8, x + 9, color);
			break;
		case 10:
			_fillRange(pixels, pixelWidth, x + 6, x + 11, color);
			break;
		case 11:
			_fillRange(pixels, pixelWidth, x + 5, x + 6, color);
			_fillRange(pixels, pixelWidth, x + 11, x + 12, color);
			break;
		default:
			break;
	}
}

void WifiIcon::_drawDot(int x, int sourceY, std::uint16_t* pixels, int pixelWidth, std::uint16_t color) const {
	if (sourceY == 13 || sourceY == 14) {
		_fillRange(pixels, pixelWidth, x + 8, x + 9, color);
	}
}

} // namespace lexxos::app::picocalc_lcd_console::detail
