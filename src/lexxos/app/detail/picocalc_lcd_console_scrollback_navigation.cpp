#include <lexxos/app/detail/picocalc_lcd_console_scrollback_navigation.h>

namespace lexxos::app::picocalc_lcd_console::detail {

LcdConsoleScrollbackViewChange LcdConsoleScrollbackNavigation::moveVertical(
    LcdConsoleViewport& viewport,
    LcdConsoleScrollback const& scrollback,
    int deltaLines) {
	if (!viewport.move(deltaLines, maxTopLine(scrollback))) {
		return {};
	}
	return {
	    .changed = true,
	    .resetHardwareScroll = true,
	    .hideStatusBar = true,
	};
}

LcdConsoleScrollbackViewChange LcdConsoleScrollbackNavigation::setTop(
    LcdConsoleViewport& viewport,
    LcdConsoleScrollback const& scrollback,
    int topLine) {
	if (!viewport.setTop(topLine, maxTopLine(scrollback))) {
		return {};
	}
	return {
	    .changed = true,
	    .resetHardwareScroll = true,
	};
}

LcdConsoleScrollbackViewChange LcdConsoleScrollbackNavigation::moveHorizontal(
    LcdConsoleViewport& viewport,
    LcdConsoleLayout const& layout,
    int deltaColumns) {
	if (!viewport.moveLeft(deltaColumns, maxLeftColumn(layout))) {
		return {};
	}
	return {
	    .changed = true,
	    .resetHardwareScroll = true,
	};
}

LcdConsoleScrollbackViewChange LcdConsoleScrollbackNavigation::setLeft(
    LcdConsoleViewport& viewport,
    LcdConsoleLayout const& layout,
    int leftColumn) {
	if (!viewport.setLeft(leftColumn, maxLeftColumn(layout))) {
		return {};
	}
	return {
	    .changed = true,
	    .resetHardwareScroll = true,
	};
}

LcdConsoleScrollbackViewChange LcdConsoleScrollbackNavigation::resetView(
    LcdConsoleViewport& viewport,
    LcdConsoleScrollback const& scrollback,
    LcdConsoleLayout const& layout) {
	bool const changedTop = viewport.setTop(maxTopLine(scrollback), maxTopLine(scrollback));
	bool const changedLeft = viewport.setLeft(0, maxLeftColumn(layout));
	if (!changedTop && !changedLeft) {
		return {};
	}
	return {
	    .changed = true,
	    .resetHardwareScroll = true,
	};
}

void LcdConsoleScrollbackNavigation::followTail(LcdConsoleViewport& viewport, LcdConsoleScrollback const& scrollback) {
	viewport.followTail(maxTopLine(scrollback));
}

int LcdConsoleScrollbackNavigation::maxTopLine(LcdConsoleScrollback const& scrollback) {
	return static_cast<int>(scrollback.count());
}

int LcdConsoleScrollbackNavigation::maxLeftColumn(LcdConsoleLayout const& layout) {
	return layout.maxViewLeftColumn();
}

int LcdConsoleScrollbackNavigation::leftColumn(LcdConsoleViewport const& viewport, LcdConsoleLayout const& layout) {
	return viewport.left(maxLeftColumn(layout));
}

bool LcdConsoleScrollbackNavigation::isActive(LcdConsoleViewport const& viewport) {
	return viewport.isActive() || viewport.isHorizontallyActive();
}

} // namespace lexxos::app::picocalc_lcd_console::detail
