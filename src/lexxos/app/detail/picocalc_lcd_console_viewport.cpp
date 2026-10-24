#include <lexxos/app/detail/picocalc_lcd_console_viewport.h>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleViewport::reset() {
	_topLine = 0;
	_leftColumn = 0;
	_followTail = true;
	_manualHorizontalView = false;
}

bool LcdConsoleViewport::move(int deltaLines, int maxTopLine) {
	return setTop(top(maxTopLine) + deltaLines, maxTopLine);
}

bool LcdConsoleViewport::setTop(int topLine, int maxTopLine) {
	if (topLine < 0) {
		topLine = 0;
	}
	if (topLine > maxTopLine) {
		topLine = maxTopLine;
	}

	bool const followTail = topLine >= maxTopLine;
	if (_topLine == topLine && _followTail == followTail) {
		return false;
	}

	_topLine = topLine;
	_followTail = followTail;
	return true;
}

void LcdConsoleViewport::followTail(int maxTopLine) {
	_topLine = maxTopLine;
	_followTail = true;
}

void LcdConsoleViewport::shiftAfterDroppedOldest() {
	if (!_followTail && _topLine > 0) {
		--_topLine;
	}
}

int LcdConsoleViewport::top(int maxTopLine) const {
	return _followTail ? maxTopLine : _topLine;
}

bool LcdConsoleViewport::moveLeft(int deltaColumns, int maxLeftColumn) {
	return setLeft(left(maxLeftColumn) + deltaColumns, maxLeftColumn);
}

bool LcdConsoleViewport::setLeft(int leftColumn, int maxLeftColumn) {
	return setLeft(leftColumn, maxLeftColumn, true);
}

bool LcdConsoleViewport::followCursorColumn(int cursorColumn, int visibleColumns, int maxLeftColumn) {
	if (visibleColumns <= 0 || maxLeftColumn <= 0) {
		return setLeft(0, maxLeftColumn, false);
	}
	if (cursorColumn < 0) {
		cursorColumn = 0;
	}

	auto const currentLeft = left(maxLeftColumn);
	auto const currentRight = currentLeft + visibleColumns - 1;
	if (cursorColumn >= currentLeft && cursorColumn <= currentRight) {
		_manualHorizontalView = false;
		return false;
	}

	auto margin = cursorFollowMarginColumns;
	if (visibleColumns <= margin * 2 + 1) {
		margin = visibleColumns > 2 ? 1 : 0;
	}

	int targetLeft = currentLeft;
	if (cursorColumn < currentLeft) {
		targetLeft = cursorColumn - margin;
	} else {
		targetLeft = cursorColumn - visibleColumns + 1 + margin;
	}
	return setLeft(targetLeft, maxLeftColumn, false);
}

bool LcdConsoleViewport::setLeft(int leftColumn, int maxLeftColumn, bool manual) {
	if (leftColumn < 0) {
		leftColumn = 0;
	}
	if (leftColumn > maxLeftColumn) {
		leftColumn = maxLeftColumn;
	}
	bool const manualHorizontalView = manual && leftColumn != 0;
	if (_leftColumn == leftColumn && _manualHorizontalView == manualHorizontalView) {
		return false;
	}

	_leftColumn = leftColumn;
	_manualHorizontalView = manualHorizontalView;
	return true;
}

int LcdConsoleViewport::left(int maxLeftColumn) const {
	if (_leftColumn < 0) {
		return 0;
	}
	return _leftColumn > maxLeftColumn ? maxLeftColumn : _leftColumn;
}

bool LcdConsoleViewport::isFollowingTail() const {
	return _followTail;
}

bool LcdConsoleViewport::isActive() const {
	return !_followTail;
}

bool LcdConsoleViewport::isHorizontallyActive() const {
	return _manualHorizontalView && _leftColumn != 0;
}

} // namespace lexxos::app::picocalc_lcd_console::detail
