#pragma once

#include <cstdint>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>
#include <lexxos/app/detail/picocalc_lcd_console_icon.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class BatteryIcon final : public Icon {
public:
	[[nodiscard]] int width() const override;

	void draw(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status,
	    int x,
	    int rowY,
	    std::uint16_t* pixels,
	    int pixelWidth) const override;

private:
	[[nodiscard]] std::uint8_t _batteryPercent(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const;

	[[nodiscard]] std::uint16_t _outlineColor(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const;

	[[nodiscard]] std::uint16_t _fillColor(std::uint8_t percent) const;

	void _drawChargingPlus(int bodyX, int bodyY, int rowY, std::uint16_t* pixels, int pixelWidth) const;
};

} // namespace lexxos::app::picocalc_lcd_console::detail
