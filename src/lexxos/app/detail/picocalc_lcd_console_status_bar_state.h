#pragma once

#include <lexxos/app/picocalc_lcd_console.h>

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleStatusBarState {
public:
	void set(lexxos::app::picocalc_lcd_console::StatusBarInfo const& info);

	void markDirty();

	[[nodiscard]] bool takeDirty();

	[[nodiscard]] lexxos::app::picocalc_lcd_console::StatusBarInfo snapshot() const;

private:
	[[nodiscard]] static lexxos::app::picocalc_lcd_console::StatusBarInfo _normalize(
	    lexxos::app::picocalc_lcd_console::StatusBarInfo info);

	[[nodiscard]] bool _changed(
	    lexxos::app::picocalc_lcd_console::StatusBarInfo const& normalized) const;

	lexxos::app::picocalc_lcd_console::StatusBarInfo _status{};
	bool _dirty{true};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
