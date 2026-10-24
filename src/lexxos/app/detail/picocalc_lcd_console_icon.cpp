#include <lexxos/app/detail/picocalc_lcd_console_icon.h>

namespace lexxos::app::picocalc_lcd_console::detail {

void Icon::_setPixel(std::uint16_t* pixels, int width, int x, std::uint16_t color) const {
	if (pixels != nullptr && x >= 0 && x < width) {
		pixels[x] = color;
	}
}

void Icon::_fillRange(std::uint16_t* pixels, int width, int x0, int x1, std::uint16_t color) const {
	if (pixels == nullptr || width <= 0) {
		return;
	}
	if (x0 < 0) {
		x0 = 0;
	}
	if (x1 >= width) {
		x1 = width - 1;
	}
	for (int x = x0; x <= x1; ++x) {
		pixels[x] = color;
	}
}

} // namespace lexxos::app::picocalc_lcd_console::detail
