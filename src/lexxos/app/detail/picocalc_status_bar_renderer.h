#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/app/detail/picocalc_lcd_console_battery_icon.h>
#include <lexxos/app/detail/picocalc_lcd_console_config.h>
#include <lexxos/app/detail/picocalc_lcd_console_font.h>
#include <lexxos/app/detail/picocalc_lcd_console_wifi_icon.h>
#include <lexxos/app/picocalc_lcd_console.h>
#include <lexxos/devices/display_device.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class PicoCalcStatusBarRenderer {
public:
	void draw(lexxos::devices::DisplayDevice& display, lexxos::app::picocalc_lcd_console::StatusBarInfo const& status);

private:
	enum class ComponentKind : std::uint8_t {
		Text,
		Icon,
		Separator,
		Spring,
	};

	struct Component {
		ComponentKind kind{ComponentKind::Text};
		Icon const* icon{nullptr};
		int x{0};
		int width{0};
		int weight{0};
		std::uint16_t color{statusTextColor};
		char text[10]{};
	};

	struct Layout {
		std::array<Component, 8> components{};
		std::size_t count{0};
		bool valid{false};
	};

	[[nodiscard]] Layout _layout(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status, int width) const;

	void _appendTime(Layout& layout, lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const;

	void _appendBatteryStatus(Layout& layout, lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const;

	void _appendWifiStatus(Layout& layout, lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const;

	void _pushText(Layout& layout, char const* text, std::uint16_t color) const;

	void _pushIcon(Layout& layout, Icon const& icon) const;

	void _pushSeparator(Layout& layout) const;

	void _pushSpring(Layout& layout, int weight) const;

	[[nodiscard]] bool _resolveSprings(Layout& layout, int width) const;

	void _drawComponentsRow(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status,
	    Layout const& layout,
	    int rowY,
	    std::uint16_t* pixels,
	    int width) const;

	void _formatBatteryPercent(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status, char (&text)[5]) const;

	void _formatTimeText(
	    lexxos::app::picocalc_lcd_console::StatusBarInfo const& status,
	    char (&text)[9]) const;

	[[nodiscard]] int _textWidth(char const* text) const;

	[[nodiscard]] std::uint16_t _batteryTextColor(
	    lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const;

	void _drawTextRow(char const* text, int x, int y, int rowY, std::uint16_t color, std::uint16_t* pixels, int width) const;

	void _drawGlyphRow(char character, int x, int y, int rowY, std::uint16_t color, std::uint16_t* pixels, int width) const;

	void _drawSeparatorRow(int x, int rowY, std::uint16_t* pixels, int width) const;

	void _setPixel(std::uint16_t* pixels, int width, int x, std::uint16_t color) const;

	void _fillRange(std::uint16_t* pixels, int width, int x0, int x1, std::uint16_t color) const;

	BatteryIcon _batteryIcon{};
	WifiIcon _wifiIcon{};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
