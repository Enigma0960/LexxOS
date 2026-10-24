#pragma once

#include <cstdint>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>
#include <lexxos/app/detail/picocalc_lcd_console_csi_state.h>
#include <lexxos/app/detail/picocalc_lcd_console_layout.h>
#include <lexxos/app/detail/picocalc_lcd_console_screen_buffer.h>
#include <lexxos/app/detail/picocalc_lcd_console_scrollback.h>
#include <lexxos/app/detail/picocalc_lcd_console_scrollback_navigation.h>
#include <lexxos/app/detail/picocalc_lcd_console_viewport.h>

namespace lexxos::app::picocalc_lcd_console::detail {

enum class LcdConsoleEscapeState : std::uint8_t {
	None,
	Escape,
	Csi,
};

struct LcdConsoleTerminalRenderHooks {
	void* context{nullptr};
	void (*markDirtyRow)(void* context, int row){nullptr};
	void (*clearDirtyRows)(void* context){nullptr};
	void (*setScrollRedrawRow)(void* context, int row){nullptr};
	bool (*hardwareScrollEnabled)(void* context){nullptr};
	void (*flushDirtyRowsBeforeHardwareScroll)(void* context, int rows){nullptr};
	bool (*advanceHardwareScroll)(void* context, int pixels){nullptr};
	void (*shiftDirtyRowsAfterHardwareScroll)(void* context, int rows){nullptr};
};

class LcdConsoleTerminalState final {
public:
	void setRenderHooks(LcdConsoleTerminalRenderHooks hooks);

	void reset(LcdConsoleLayout const& layout);

	void clear();

	void markCursorDirty(bool textMode);

	void writeCharacterWithoutCursor(char value);

	void onTextModeEnabled();

	[[nodiscard]] LcdConsoleScrollbackViewChange moveScrollbackView(int deltaLines);

	[[nodiscard]] LcdConsoleScrollbackViewChange setScrollbackViewTop(int topLine);

	[[nodiscard]] LcdConsoleScrollbackViewChange moveHorizontalView(int deltaColumns);

	[[nodiscard]] LcdConsoleScrollbackViewChange setHorizontalViewLeft(int leftColumn);

	[[nodiscard]] LcdConsoleScrollbackViewChange resetView();

	[[nodiscard]] bool isScrollbackActive() const;

	[[nodiscard]] int maxViewTopLine() const;

	[[nodiscard]] int maxViewLeftColumn() const;

	[[nodiscard]] int viewLeftColumn() const;

	[[nodiscard]] int viewTopLine() const;

	[[nodiscard]] bool viewFollowTail() const;

	[[nodiscard]] int cursorX() const;

	[[nodiscard]] int cursorY() const;

	[[nodiscard]] bool cursorVisible() const;

	[[nodiscard]] LcdConsoleLayout const& layout() const;

	[[nodiscard]] LcdConsoleScreenBuffer& screen();

	[[nodiscard]] LcdConsoleScreenBuffer const& screen() const;

	[[nodiscard]] LcdConsoleScrollback const& scrollback() const;

	[[nodiscard]] LcdConsoleViewport const& viewport() const;

private:
	void _handleCsi(char finalByte);

	void _handlePrivateCsi(char finalByte);

	void _handleSgr();

	void _resetTextAttributes();

	void _setForegroundPalette(std::uint8_t paletteIndex);

	void _setBackgroundPalette(std::uint8_t paletteIndex);

	void _putPrintable(char value);

	void _newLine();

	void _backspace();

	void _eraseLine(int mode);

	void _eraseDisplay(int mode);

	void _clearScreen();

	void _moveCursorLeft(int count);

	void _moveCursorRight(int count);

	void _moveCursorUp(int count);

	void _moveCursorDown(int count);

	void _setCursorPosition(int row, int column);

	void _setCursorRow(int row);

	void _setCursorColumn(int column);

	void _scrollUp();

	void _markDirtyRow(int row);

	void _markAllRowsDirty();

	void _clearDirtyRows();

	void _setScrollRedrawRow(int row);

	void _markCellDirty(int column, int row);

	void _invalidateView();

	void _followScrollbackTail();

	void _followCursorColumn();

	[[nodiscard]] bool _hardwareScrollEnabled() const;

	void _flushDirtyRowsBeforeHardwareScroll(int rows);

	[[nodiscard]] bool _advanceHardwareScroll(int pixels);

	void _shiftDirtyRowsAfterHardwareScroll(int rows);

	LcdConsoleTerminalRenderHooks _hooks{};
	LcdConsoleLayout _layout{};
	LcdConsoleScreenBuffer _screen{};
	LcdConsoleScrollback _scrollback{};
	LcdConsoleViewport _viewport{};
	int _cursorX{0};
	int _cursorY{0};
	bool _wrapPending{false};
	int _savedCursorX{0};
	int _savedCursorY{0};
	LcdConsoleCsiState _csi{};
	LcdConsoleEscapeState _escapeState{LcdConsoleEscapeState::None};
	std::uint8_t _currentAttribute{defaultCellAttribute};
	bool _sgrBright{false};
	bool _cursorVisible{true};
};

} // namespace lexxos::app::picocalc_lcd_console::detail
