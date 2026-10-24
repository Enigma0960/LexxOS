#pragma once

#include <lexxos/app/detail/picocalc_lcd_console_layout.h>
#include <lexxos/app/detail/picocalc_lcd_console_scrollback.h>
#include <lexxos/app/detail/picocalc_lcd_console_viewport.h>

namespace lexxos::app::picocalc_lcd_console::detail {

struct LcdConsoleScrollbackViewChange {
	bool changed{false};
	bool resetHardwareScroll{false};
	bool hideStatusBar{false};
};

class LcdConsoleScrollbackNavigation final {
public:
	[[nodiscard]] static LcdConsoleScrollbackViewChange moveVertical(
	    LcdConsoleViewport& viewport,
	    LcdConsoleScrollback const& scrollback,
	    int deltaLines);

	[[nodiscard]] static LcdConsoleScrollbackViewChange setTop(
	    LcdConsoleViewport& viewport,
	    LcdConsoleScrollback const& scrollback,
	    int topLine);

	[[nodiscard]] static LcdConsoleScrollbackViewChange moveHorizontal(
	    LcdConsoleViewport& viewport,
	    LcdConsoleLayout const& layout,
	    int deltaColumns);

	[[nodiscard]] static LcdConsoleScrollbackViewChange setLeft(
	    LcdConsoleViewport& viewport,
	    LcdConsoleLayout const& layout,
	    int leftColumn);

	[[nodiscard]] static LcdConsoleScrollbackViewChange resetView(
	    LcdConsoleViewport& viewport,
	    LcdConsoleScrollback const& scrollback,
	    LcdConsoleLayout const& layout);

	static void followTail(LcdConsoleViewport& viewport, LcdConsoleScrollback const& scrollback);

	[[nodiscard]] static int maxTopLine(LcdConsoleScrollback const& scrollback);

	[[nodiscard]] static int maxLeftColumn(LcdConsoleLayout const& layout);

	[[nodiscard]] static int leftColumn(LcdConsoleViewport const& viewport, LcdConsoleLayout const& layout);

	[[nodiscard]] static bool isActive(LcdConsoleViewport const& viewport);
};

} // namespace lexxos::app::picocalc_lcd_console::detail
