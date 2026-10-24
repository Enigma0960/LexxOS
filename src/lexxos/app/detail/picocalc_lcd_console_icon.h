#pragma once

#include <cstdint>

#include <lexxos/app/picocalc_lcd_console.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class Icon {
public:
	virtual ~Icon() = default;

	[[nodiscard]] virtual int width() const = 0;
	virtual void draw(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status,
	    int x,
	    int rowY,
	    std::uint16_t* pixels,
	    int pixelWidth) const = 0;

protected:
	void _setPixel(std::uint16_t* pixels, int width, int x, std::uint16_t color) const;

	void _fillRange(std::uint16_t* pixels, int width, int x0, int x1, std::uint16_t color) const;
};

} // namespace lexxos::app::picocalc_lcd_console::detail
