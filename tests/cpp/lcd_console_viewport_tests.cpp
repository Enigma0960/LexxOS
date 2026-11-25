#include <cstdlib>
#include <iostream>
#include <string_view>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>
#include <lexxos/app/detail/picocalc_lcd_console_layout.h>
#include <lexxos/app/detail/picocalc_lcd_console_terminal_state.h>
#include <lexxos/devices/display_device.h>

namespace {

class FakeDisplay final : public lexxos::devices::DisplayDevice {
public:
	[[nodiscard]] std::string_view name() const override {
		return "fake-lcd";
	}

	[[nodiscard]] bool isReady() const override {
		return true;
	}

	[[nodiscard]] int width() const override {
		return 12 * lexxos::app::picocalc_lcd_console::detail::cellWidth;
	}

	[[nodiscard]] int height() const override {
		return lexxos::app::picocalc_lcd_console::detail::statusBarHeightPixels
		    + 4 * lexxos::app::picocalc_lcd_console::detail::cellHeight;
	}

	[[nodiscard]] lexxos::devices::PixelFormat nativeFormat() const override {
		return lexxos::devices::PixelFormat::Rgb565;
	}

	bool clear(std::uint16_t) override {
		return true;
	}

	bool beginFrame() override {
		return true;
	}

	bool blitRgb565(lexxos::devices::Rect const&, std::uint16_t const*, int) override {
		return true;
	}

	bool endFrame() override {
		return true;
	}
};

struct HookState {
	int dirtyRows{0};
	int clearDirtyRows{0};
	int scrollRedrawRow{-2};
};

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "lcd console viewport test failed: " << message << '\n';
		std::exit(1);
	}
}

void markDirtyRow(void* context, int) {
	auto& state = *static_cast<HookState*>(context);
	++state.dirtyRows;
}

void clearDirtyRows(void* context) {
	auto& state = *static_cast<HookState*>(context);
	++state.clearDirtyRows;
}

void setScrollRedrawRow(void* context, int row) {
	auto& state = *static_cast<HookState*>(context);
	state.scrollRedrawRow = row;
}

lexxos::app::picocalc_lcd_console::detail::LcdConsoleLayout testLayout() {
	FakeDisplay display;
	lexxos::app::picocalc_lcd_console::detail::LcdConsoleLayout layout;
	expect(layout.configure(display), "fake display should produce a valid layout");
	expect(layout.visibleColumns() == 12, "test layout should have 12 visible columns");
	expect(layout.rows() == 4, "test layout should have 4 rows");
	return layout;
}

lexxos::app::picocalc_lcd_console::detail::LcdConsoleTerminalState testTerminal(HookState& hooks) {
	lexxos::app::picocalc_lcd_console::detail::LcdConsoleTerminalState terminal;
	terminal.setRenderHooks({
	    .context = &hooks,
	    .markDirtyRow = markDirtyRow,
	    .clearDirtyRows = clearDirtyRows,
	    .setScrollRedrawRow = setScrollRedrawRow,
	});
	terminal.reset(testLayout());
	return terminal;
}

void feed(lexxos::app::picocalc_lcd_console::detail::LcdConsoleTerminalState& terminal, std::string_view text) {
	for (char const character : text) {
		terminal.writeCharacterWithoutCursor(character);
	}
}

void feedCursorLeft(lexxos::app::picocalc_lcd_console::detail::LcdConsoleTerminalState& terminal, int count) {
	for (int index = 0; index < count; ++index) {
		feed(terminal, "\x1B[D");
	}
}

void checkCursorFollowShiftsRight() {
	HookState hooks;
	auto terminal = testTerminal(hooks);

	feed(terminal, "abcdefghijkl");

	expect(terminal.cursorX() == 12, "cursor should be after the typed text");
	expect(terminal.viewLeftColumn() > 0, "cursor follow should shift the live viewport right");
	expect(terminal.cursorX() >= terminal.viewLeftColumn(), "cursor should not be left of the live viewport");
	expect(
	    terminal.cursorX() < terminal.viewLeftColumn() + terminal.layout().visibleColumns(),
	    "cursor should stay visible at the right edge");
	expect(!terminal.isScrollbackActive(), "automatic cursor follow should not count as manual scrollback");
	expect(hooks.dirtyRows > 0, "horizontal viewport change should invalidate rows");
}

void checkCursorFollowReturnsLeft() {
	HookState hooks;
	auto terminal = testTerminal(hooks);

	feed(terminal, "abcdefghijklmnopqrstuvwx");
	expect(terminal.viewLeftColumn() > 0, "long text should shift viewport before left movement");
	feedCursorLeft(terminal, 24);

	expect(terminal.cursorX() == 0, "cursor should move back to the beginning");
	expect(terminal.viewLeftColumn() == 0, "cursor follow should shift the live viewport back left");
}

void checkCarriageReturnResetsHorizontalView() {
	HookState hooks;
	auto terminal = testTerminal(hooks);

	feed(terminal, "abcdefghijklmnop");
	expect(terminal.viewLeftColumn() > 0, "long text should shift viewport before carriage return");
	feed(terminal, "\r");

	expect(terminal.cursorX() == 0, "carriage return should move cursor to column zero");
	expect(terminal.viewLeftColumn() == 0, "carriage return should restore the visible prompt edge");
}

void checkWrapToVisibleColumns() {
	HookState hooks;
	auto terminal = testTerminal(hooks);
	terminal.setWrapToVisibleColumns(true);

	feed(terminal, "abcdefghijklmnopqrstuvwxy");
	expect(terminal.viewLeftColumn() == 0, "soft-wrapped output must not pan the viewport");
	expect(terminal.cursorY() == 2 && terminal.cursorX() == 1, "25 characters on a 12-column display use three rows");
	expect(terminal.screen().cell(0, 0) == 'a', "first row must keep its first character");
	expect(terminal.screen().cell(11, 0) == 'l', "first row must end at visible column 11");
	expect(terminal.screen().cell(0, 1) == 'm', "second row must start with the next character");
	expect(terminal.screen().cell(11, 1) == 'x', "second row must end at visible column 11");
	expect(terminal.screen().cell(0, 2) == 'y', "third row must start with the last character");
}

void checkCrLfAtWrapBoundary() {
	HookState hooks;
	auto terminal = testTerminal(hooks);
	terminal.setWrapToVisibleColumns(true);

	feed(terminal, "abcdefghijkl\r\nZ");
	expect(terminal.cursorY() == 1 && terminal.cursorX() == 1, "CR/LF after exactly 12 columns must advance one row");
	expect(terminal.screen().cell(0, 1) == 'Z', "following line should begin in column zero");
}

void checkExplicitColumnIsClampedToVisibleWidth() {
	HookState hooks;
	auto terminal = testTerminal(hooks);
	terminal.setWrapToVisibleColumns(true);

	feed(terminal, "\x1B[200GQ");
	expect(terminal.viewLeftColumn() == 0, "ANSI cursor motion must not pan wrapped output");
	expect(terminal.screen().cell(11, 0) == 'Q', "ANSI column 200 must clamp to column 12");
}

void checkSgrAttributesAreStoredPerCell() {
	HookState hooks;
	auto terminal = testTerminal(hooks);

	feed(terminal, "\x1B[31mX\x1B[0mY");

	expect(
	    (terminal.screen().attribute(0, 0) & 0x0FU) == lexxos::app::picocalc_lcd_console::detail::paletteRed,
	    "SGR red should be stored on the first cell");
	expect(
	    terminal.screen().attribute(1, 0) == lexxos::app::picocalc_lcd_console::detail::defaultCellAttribute,
	    "SGR reset should restore default cell attributes");
}

void checkSoftwareScrollFlushBudget() {
	namespace config = lexxos::app::picocalc_lcd_console::detail;
	expect(config::maxScrollRowsPerFlush >= 37, "tail view must publish a complete page");
	expect(config::charactersPerFlush(100, false) > 0, "interactive output must advance");
	expect(config::charactersPerFlush(6000, false) > config::charactersPerFlush(100, false), "backlog must drain faster than normal output");
	expect(config::charactersPerFlush(6000, true) > 0, "page invalidation must not stop text ingestion");
	HookState hooks;
	auto terminal = testTerminal(hooks);
	terminal.setWrapToVisibleColumns(true);
	feed(terminal, "first\nsecond\nthird\nfourth\nfifth\n");
	expect(hooks.scrollRedrawRow == 0, "software scroll must request repaint");
	auto const history = terminal.scrollback().count();
	expect(history > 0 && terminal.viewFollowTail(), "batched output must preserve history and follow tail");
}

void checkRowRotationAndHistory() {
	namespace lcd = lexxos::app::picocalc_lcd_console::detail;
	lcd::LcdConsoleScreenBuffer screen;
	screen.clearCells();
	screen.clearDirtyCells();
	for (int row = 0; row < 4; ++row) {
		for (int x = 0; x < 12; ++x) {
			screen.cell(x, row) = static_cast<char>('A' + row);
			screen.attribute(x, row) = static_cast<std::uint8_t>(row + 1);
		}
	}

	for (int turn = 0; turn < 40; ++turn) {
		char cells[3][12]{};
		std::uint8_t attributes[3][12]{};
		for (int row = 0; row < 3; ++row) {
			for (int x = 0; x < 12; ++x) {
				cells[row][x] = screen.cell(x, row + 1);
				attributes[row][x] = screen.attribute(x, row + 1);
			}
		}

		screen.advanceLine(12, 4);
		for (int row = 0; row < 3; ++row) {
			for (int x = 0; x < 12; ++x) {
				expect(screen.cell(x, row) == cells[row][x] && screen.attribute(x, row) == attributes[row][x], "row rotation repeated stale cells or attributes");
			}
		}

		for (int x = 0; x < 12; ++x) {
			expect(screen.cell(x, 3) == ' ' && screen.attribute(x, 3) == lcd::defaultCellAttribute, "reused row must be blank");
			screen.cell(x, 3) = static_cast<char>('a' + turn % 26);
			screen.attribute(x, 3) = static_cast<std::uint8_t>(turn);
		}
	}

	screen.clearCells();
	expect(screen.cell(0, 0) == ' ', "clear did not reset row ring");
	HookState hooks;
	auto terminal = testTerminal(hooks);
	for (char c = 'A'; c <= 'T'; ++c) {
		terminal.writeCharacterWithoutCursor(c);
		feed(terminal, "\r\n");
	}

	expect(terminal.screen().cell(0, 0) == 'R' && terminal.screen().cell(0, 1) == 'S' && terminal.screen().cell(0, 2) == 'T' && terminal.screen().cell(0, 3) == ' ', "tail contains repeated old rows");
	expect(terminal.scrollback().count() == 17 && terminal.scrollback().cell(0, 0, 12) == 'A' && terminal.scrollback().cell(16, 0, 12) == 'Q', "history lost order after row-ring wraps");
	(void) terminal.moveScrollbackView(-4);
	expect(!terminal.viewFollowTail(), "manual navigation must detach from tail");
	auto const top = terminal.viewTopLine();
	feed(terminal, "U\r\n");
	expect(terminal.viewTopLine() == top, "new output moved a manually selected page");
	(void) terminal.resetView();
	expect(terminal.viewFollowTail(), "return to tail failed");
}

} // namespace

int main() {
	checkCursorFollowShiftsRight();
	checkCursorFollowReturnsLeft();
	checkCarriageReturnResetsHorizontalView();
	checkWrapToVisibleColumns();
	checkCrLfAtWrapBoundary();
	checkExplicitColumnIsClampedToVisibleWidth();
	checkSgrAttributesAreStoredPerCell();
	checkSoftwareScrollFlushBudget();
	checkRowRotationAndHistory();
	return 0;
}
