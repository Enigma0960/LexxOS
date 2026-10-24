#pragma once

#include <cstdint>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>
#include <lexxos/app/detail/picocalc_lcd_console_icon.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class WifiIcon final : public Icon {
public:
	[[nodiscard]] int width() const override;

	void draw(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status,
	    int x,
	    int rowY,
	    std::uint16_t* pixels,
	    int pixelWidth) const override;

private:
	void _drawHighArc(int x, int sourceY, std::uint16_t* pixels, int pixelWidth, std::uint16_t color) const;

	void _drawMidArc(int x, int sourceY, std::uint16_t* pixels, int pixelWidth, std::uint16_t color) const;

	void _drawLowArc(int x, int sourceY, std::uint16_t* pixels, int pixelWidth, std::uint16_t color) const;

	void _drawDot(int x, int sourceY, std::uint16_t* pixels, int pixelWidth, std::uint16_t color) const;
};

} // namespace lexxos::app::picocalc_lcd_console::detail
