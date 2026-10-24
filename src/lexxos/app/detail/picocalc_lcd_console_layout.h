#pragma once

namespace lexxos::devices {
class DisplayDevice;
} // namespace lexxos::devices

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleLayout final {
public:
	void reset();

	[[nodiscard]] bool configure(lexxos::devices::DisplayDevice const& display);

	[[nodiscard]] bool valid() const;

	[[nodiscard]] int columns() const;

	[[nodiscard]] int visibleColumns() const;

	[[nodiscard]] int rows() const;

	[[nodiscard]] int maxViewLeftColumn() const;

private:
	int _columns{0};
	int _visibleColumns{0};
	int _rows{0};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
