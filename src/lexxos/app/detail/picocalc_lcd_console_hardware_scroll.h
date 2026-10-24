#pragma once

#include <lexxos/devices/display_device.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleHardwareScroll {
public:
	void resetState(int rows);

	void configure(lexxos::devices::DisplayDevice* display, int rows);

	void reset(lexxos::devices::DisplayDevice* display, bool resetScroll);

	[[nodiscard]] bool advance(lexxos::devices::DisplayDevice* display, int pixels);

	[[nodiscard]] int rowTopPixel(int row) const;

	[[nodiscard]] bool enabled() const;

	[[nodiscard]] int areaPixels() const;

	[[nodiscard]] int startPixels() const;

private:
	bool _enabled{false};
	int _areaPixels{0};
	int _startPixels{0};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
