#include <lexxos/app/detail/picocalc_lcd_console_config.h>
#include <lexxos/app/detail/picocalc_lcd_console_layout.h>
#include <lexxos/app/detail/picocalc_lcd_console_terminal_state.h>
#include <lexxos/devices/display_device.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

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

} // namespace

int main() {
	checkCursorFollowShiftsRight();
	checkCursorFollowReturnsLeft();
	checkCarriageReturnResetsHorizontalView();
	checkSgrAttributesAreStoredPerCell();
	return 0;
}
