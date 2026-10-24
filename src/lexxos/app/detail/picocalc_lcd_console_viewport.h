#pragma once

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleViewport {
public:
	void reset();

	[[nodiscard]] bool move(int deltaLines, int maxTopLine);

	[[nodiscard]] bool setTop(int topLine, int maxTopLine);

	void followTail(int maxTopLine);

	void shiftAfterDroppedOldest();

	[[nodiscard]] int top(int maxTopLine) const;

	[[nodiscard]] bool moveLeft(int deltaColumns, int maxLeftColumn);

	[[nodiscard]] bool setLeft(int leftColumn, int maxLeftColumn);

	[[nodiscard]] bool followCursorColumn(int cursorColumn, int visibleColumns, int maxLeftColumn);

	[[nodiscard]] int left(int maxLeftColumn) const;

	[[nodiscard]] bool isFollowingTail() const;

	[[nodiscard]] bool isActive() const;

	[[nodiscard]] bool isHorizontallyActive() const;

private:
	[[nodiscard]] bool setLeft(int leftColumn, int maxLeftColumn, bool manual);

	int _topLine{0};
	int _leftColumn{0};
	bool _followTail{true};
	bool _manualHorizontalView{false};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
