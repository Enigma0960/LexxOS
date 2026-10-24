#include <lexxos/app/detail/picocalc_lcd_console_terminal_state.h>

namespace {

std::uint8_t ansiPalette(int offset, bool bright) {
	if (offset < 0) {
		offset = 0;
	} else if (offset > 7) {
		offset = 7;
	}
	return static_cast<std::uint8_t>(offset + (bright ? lexxos::app::picocalc_lcd_console::detail::paletteBrightBlack : 0));
}

} // namespace

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleTerminalState::setRenderHooks(LcdConsoleTerminalRenderHooks hooks) {
	_hooks = hooks;
}

void LcdConsoleTerminalState::reset(LcdConsoleLayout const& layout) {
	_layout = layout;
	_screen.clearCells();
	_screen.clearDirtyCells();
	_cursorX = 0;
	_cursorY = 0;
	_wrapPending = false;
	_savedCursorX = 0;
	_savedCursorY = 0;
	_escapeState = LcdConsoleEscapeState::None;
	_csi.reset();
	_scrollback.reset();
	_viewport.reset();
	_resetTextAttributes();
	_cursorVisible = true;
}

void LcdConsoleTerminalState::clear() {
	_screen.clearCells();
	_screen.clearDirtyCells();
	_cursorX = 0;
	_cursorY = 0;
	_wrapPending = false;
	_savedCursorX = 0;
	_savedCursorY = 0;
	_escapeState = LcdConsoleEscapeState::None;
	_csi.reset();
	_resetTextAttributes();
	_followScrollbackTail();
	(void) setHorizontalViewLeft(0);
	_clearDirtyRows();
	_markAllRowsDirty();
}

void LcdConsoleTerminalState::markCursorDirty(bool textMode) {
	if (_viewport.isFollowingTail() && textMode) {
		_markCellDirty(_cursorX, _cursorY);
	}
}

void LcdConsoleTerminalState::writeCharacterWithoutCursor(char value) {
	auto const byte = static_cast<unsigned char>(value);
	if (_escapeState == LcdConsoleEscapeState::Escape) {
		if (value == '[') {
			_escapeState = LcdConsoleEscapeState::Csi;
			_csi.reset();
		} else {
			_escapeState = LcdConsoleEscapeState::None;
		}
		return;
	}

	if (_escapeState == LcdConsoleEscapeState::Csi) {
		if (_csi.consume(value)) {
			return;
		}

		if (_csi.isFinal(value)) {
			_csi.finishParameter();
			_handleCsi(value);
		}
		_escapeState = LcdConsoleEscapeState::None;
		return;
	}

	if (byte == 0x1BU) {
		_escapeState = LcdConsoleEscapeState::Escape;
		return;
	}

	switch (value) {
		case '\r':
			_cursorX = 0;
			_wrapPending = false;
			_followCursorColumn();
			return;
		case '\n':
			_wrapPending = false;
			_newLine();
			return;
		case '\b':
			_wrapPending = false;
			_backspace();
			return;
		case '\a':
			return;
		default:
			break;
	}

	if (byte < 0x20U || byte > 0x7EU) {
		return;
	}

	_putPrintable(value);
}

void LcdConsoleTerminalState::onTextModeEnabled() {
	_markAllRowsDirty();
	_followScrollbackTail();
	(void) setHorizontalViewLeft(0);
}

LcdConsoleScrollbackViewChange LcdConsoleTerminalState::moveScrollbackView(int deltaLines) {
	auto const change = LcdConsoleScrollbackNavigation::moveVertical(_viewport, _scrollback, deltaLines);
	if (change.changed) {
		_invalidateView();
	}
	return change;
}

LcdConsoleScrollbackViewChange LcdConsoleTerminalState::setScrollbackViewTop(int topLine) {
	auto const change = LcdConsoleScrollbackNavigation::setTop(_viewport, _scrollback, topLine);
	if (change.changed) {
		_invalidateView();
	}
	return change;
}

LcdConsoleScrollbackViewChange LcdConsoleTerminalState::moveHorizontalView(int deltaColumns) {
	auto const change = LcdConsoleScrollbackNavigation::moveHorizontal(_viewport, _layout, deltaColumns);
	if (change.changed) {
		_invalidateView();
	}
	return change;
}

LcdConsoleScrollbackViewChange LcdConsoleTerminalState::setHorizontalViewLeft(int leftColumn) {
	auto const change = LcdConsoleScrollbackNavigation::setLeft(_viewport, _layout, leftColumn);
	if (change.changed) {
		_invalidateView();
	}
	return change;
}

LcdConsoleScrollbackViewChange LcdConsoleTerminalState::resetView() {
	auto const change = LcdConsoleScrollbackNavigation::resetView(_viewport, _scrollback, _layout);
	if (change.changed) {
		_invalidateView();
	}
	return change;
}

bool LcdConsoleTerminalState::isScrollbackActive() const {
	return LcdConsoleScrollbackNavigation::isActive(_viewport);
}

int LcdConsoleTerminalState::maxViewTopLine() const {
	return LcdConsoleScrollbackNavigation::maxTopLine(_scrollback);
}

int LcdConsoleTerminalState::maxViewLeftColumn() const {
	return LcdConsoleScrollbackNavigation::maxLeftColumn(_layout);
}

int LcdConsoleTerminalState::viewLeftColumn() const {
	return LcdConsoleScrollbackNavigation::leftColumn(_viewport, _layout);
}

int LcdConsoleTerminalState::viewTopLine() const {
	return _viewport.top(maxViewTopLine());
}

bool LcdConsoleTerminalState::viewFollowTail() const {
	return _viewport.isFollowingTail();
}

int LcdConsoleTerminalState::cursorX() const {
	return _cursorX;
}

int LcdConsoleTerminalState::cursorY() const {
	return _cursorY;
}

bool LcdConsoleTerminalState::cursorVisible() const {
	return _cursorVisible;
}

LcdConsoleLayout const& LcdConsoleTerminalState::layout() const {
	return _layout;
}

LcdConsoleScreenBuffer& LcdConsoleTerminalState::screen() {
	return _screen;
}

LcdConsoleScreenBuffer const& LcdConsoleTerminalState::screen() const {
	return _screen;
}

LcdConsoleScrollback const& LcdConsoleTerminalState::scrollback() const {
	return _scrollback;
}

LcdConsoleViewport const& LcdConsoleTerminalState::viewport() const {
	return _viewport;
}

void LcdConsoleTerminalState::_handleCsi(char finalByte) {
	if (_csi.isPrivate()) {
		_handlePrivateCsi(finalByte);
		return;
	}

	auto const count = _csi.countParameter();
	switch (finalByte) {
		case 'K':
			_eraseLine(_csi.parameter(0, 0));
			return;
		case 'J':
			_eraseDisplay(_csi.parameter(0, 0));
			return;
		case 'D':
			_moveCursorLeft(count);
			return;
		case 'C':
			_moveCursorRight(count);
			return;
		case 'A':
			_moveCursorUp(count);
			return;
		case 'B':
			_moveCursorDown(count);
			return;
		case 'E':
			_moveCursorDown(count);
			_cursorX = 0;
			return;
		case 'F':
			_moveCursorUp(count);
			_cursorX = 0;
			return;
		case 'G':
			_setCursorColumn(_csi.parameter(0, 1));
			return;
		case 'H':
		case 'f':
			_setCursorPosition(_csi.parameter(0, 1), _csi.parameter(1, 1));
			return;
		case 'd':
			_setCursorRow(_csi.parameter(0, 1));
			return;
		case 'm':
			_handleSgr();
			return;
		case 's':
			_savedCursorX = _cursorX;
			_savedCursorY = _cursorY;
			return;
		case 'u':
			_cursorX = _savedCursorX;
			_cursorY = _savedCursorY;
			_wrapPending = false;
			_followCursorColumn();
			return;
		default:
			return;
	}
}

void LcdConsoleTerminalState::_handlePrivateCsi(char finalByte) {
	if (_csi.parameter(0, 0) != 25) {
		return;
	}

	if (finalByte == 'h') {
		_cursorVisible = true;
	} else if (finalByte == 'l') {
		_cursorVisible = false;
	}
}

void LcdConsoleTerminalState::_handleSgr() {
	auto const parameterCount = _csi.parameterCount();
	if (parameterCount <= 0) {
		_resetTextAttributes();
		return;
	}

	for (int index = 0; index < parameterCount; ++index) {
		auto const code = _csi.parameter(static_cast<std::size_t>(index), 0);
		if (code == 0) {
			_resetTextAttributes();
		} else if (code == 1) {
			_sgrBright = true;
		} else if (code == 22) {
			_sgrBright = false;
		} else if (code == 39) {
			_setForegroundPalette(defaultForegroundPalette);
		} else if (code == 49) {
			_setBackgroundPalette(defaultBackgroundPalette);
		} else if (code >= 30 && code <= 37) {
			_setForegroundPalette(ansiPalette(code - 30, _sgrBright));
		} else if (code >= 40 && code <= 47) {
			_setBackgroundPalette(ansiPalette(code - 40, false));
		} else if (code >= 90 && code <= 97) {
			_setForegroundPalette(ansiPalette(code - 90, true));
		} else if (code >= 100 && code <= 107) {
			_setBackgroundPalette(ansiPalette(code - 100, true));
		}
	}
}

void LcdConsoleTerminalState::_resetTextAttributes() {
	_currentAttribute = defaultCellAttribute;
	_sgrBright = false;
}

void LcdConsoleTerminalState::_setForegroundPalette(std::uint8_t paletteIndex) {
	_currentAttribute = static_cast<std::uint8_t>((_currentAttribute & 0xF0U) | (paletteIndex & 0x0FU));
}

void LcdConsoleTerminalState::_setBackgroundPalette(std::uint8_t paletteIndex) {
	_currentAttribute = static_cast<std::uint8_t>((_currentAttribute & 0x0FU) | ((paletteIndex & 0x0FU) << 4U));
}

void LcdConsoleTerminalState::_putPrintable(char value) {
	if (_wrapPending) {
		return;
	}

	_screen.cell(_cursorX, _cursorY) = value;
	_screen.attribute(_cursorX, _cursorY) = _currentAttribute;
	_markCellDirty(_cursorX, _cursorY);
	if (_cursorX + 1 < _layout.columns()) {
		++_cursorX;
	} else {
		_wrapPending = true;
	}
	_followCursorColumn();
}

void LcdConsoleTerminalState::_newLine() {
	_wrapPending = false;
	_cursorX = 0;
	_markDirtyRow(_cursorY);
	++_cursorY;
	if (_cursorY < _layout.rows()) {
		_followCursorColumn();
		return;
	}

	_cursorY = _layout.rows() - 1;
	_scrollUp();
	_followCursorColumn();
}

void LcdConsoleTerminalState::_backspace() {
	if (_cursorX > 0) {
		--_cursorX;
		_followCursorColumn();
		return;
	}
	if (_cursorY > 0) {
		--_cursorY;
		_cursorX = _layout.columns() - 1;
		_followCursorColumn();
	}
}

void LcdConsoleTerminalState::_eraseLine(int mode) {
	_wrapPending = false;
	int start = _cursorX;
	int end = _layout.columns() - 1;
	if (mode == 1) {
		start = 0;
		end = _cursorX;
	} else if (mode == 2) {
		start = 0;
	}

	for (int x = start; x <= end && x < _layout.columns(); ++x) {
		if (x >= 0) {
			_screen.cell(x, _cursorY) = blankCharacter;
			_screen.attribute(x, _cursorY) = defaultCellAttribute;
		}
	}
	_markDirtyRow(_cursorY);
}

void LcdConsoleTerminalState::_eraseDisplay(int mode) {
	_wrapPending = false;
	if (mode == 2 || mode == 3) {
		_clearScreen();
		return;
	}

	if (mode == 1) {
		for (int y = 0; y <= _cursorY && y < _layout.rows(); ++y) {
			auto const end = y == _cursorY ? _cursorX : _layout.columns() - 1;
			for (int x = 0; x <= end && x < _layout.columns(); ++x) {
				_screen.cell(x, y) = blankCharacter;
				_screen.attribute(x, y) = defaultCellAttribute;
			}
			_markDirtyRow(y);
		}
		return;
	}

	for (int y = _cursorY; y < _layout.rows(); ++y) {
		auto const start = y == _cursorY ? _cursorX : 0;
		for (int x = start; x < _layout.columns(); ++x) {
			_screen.cell(x, y) = blankCharacter;
			_screen.attribute(x, y) = defaultCellAttribute;
		}
		_markDirtyRow(y);
	}
}

void LcdConsoleTerminalState::_clearScreen() {
	_wrapPending = false;
	_screen.clearCells();
	_followCursorColumn();
	_markAllRowsDirty();
}

void LcdConsoleTerminalState::_moveCursorLeft(int count) {
	_wrapPending = false;
	while (count-- > 0) {
		if (_cursorX > 0) {
			--_cursorX;
		} else if (_cursorY > 0) {
			--_cursorY;
			_cursorX = _layout.columns() - 1;
		}
	}
	_followCursorColumn();
}

void LcdConsoleTerminalState::_moveCursorRight(int count) {
	_wrapPending = false;
	while (count-- > 0) {
		if (_cursorX + 1 < _layout.columns()) {
			++_cursorX;
		} else if (_cursorY + 1 < _layout.rows()) {
			_cursorX = 0;
			++_cursorY;
		}
	}
	_followCursorColumn();
}

void LcdConsoleTerminalState::_moveCursorUp(int count) {
	_wrapPending = false;
	_cursorY = count >= _cursorY ? 0 : _cursorY - count;
	_followCursorColumn();
}

void LcdConsoleTerminalState::_moveCursorDown(int count) {
	_wrapPending = false;
	_cursorY += count;
	if (_cursorY >= _layout.rows()) {
		_cursorY = _layout.rows() - 1;
	}
	_followCursorColumn();
}

void LcdConsoleTerminalState::_setCursorPosition(int row, int column) {
	_wrapPending = false;
	_setCursorRow(row);
	_setCursorColumn(column);
}

void LcdConsoleTerminalState::_setCursorRow(int row) {
	_wrapPending = false;
	if (row <= 0) {
		row = 1;
	}
	_cursorY = row - 1;
	if (_cursorY >= _layout.rows()) {
		_cursorY = _layout.rows() - 1;
	}
	_followCursorColumn();
}

void LcdConsoleTerminalState::_setCursorColumn(int column) {
	_wrapPending = false;
	if (column <= 0) {
		column = 1;
	}
	_cursorX = column - 1;
	if (_cursorX >= _layout.columns()) {
		_cursorX = _layout.columns() - 1;
	}
	_followCursorColumn();
}

void LcdConsoleTerminalState::_scrollUp() {
	bool const canHardwareScroll = _viewport.isFollowingTail() && _hardwareScrollEnabled();
	bool const canShiftDirtyRows = _hooks.shiftDirtyRowsAfterHardwareScroll != nullptr;

	if (canHardwareScroll && !canShiftDirtyRows) {
		_flushDirtyRowsBeforeHardwareScroll(_layout.rows());
	}

	auto const droppedOldest = _scrollback.storeRow(&_screen.cell(0, 0), _layout.columns());
	if (_viewport.isFollowingTail()) {
		_followScrollbackTail();
	} else if (droppedOldest) {
		_viewport.shiftAfterDroppedOldest();
	}

	for (int y = 1; y < _layout.rows(); ++y) {
		for (int x = 0; x < _layout.columns(); ++x) {
			_screen.cell(x, y - 1) = _screen.cell(x, y);
			_screen.attribute(x, y - 1) = _screen.attribute(x, y);
		}
	}

	for (int x = 0; x < _layout.columns(); ++x) {
		_screen.cell(x, _layout.rows() - 1) = blankCharacter;
		_screen.attribute(x, _layout.rows() - 1) = defaultCellAttribute;
	}

	if (canHardwareScroll && _advanceHardwareScroll(cellHeight)) {
		if (canShiftDirtyRows) {
			_shiftDirtyRowsAfterHardwareScroll(_layout.rows());
			_screen.scrollDirtyCellsUp(_layout.columns(), _layout.rows());
			_screen.clearDirtyCellsInRow(_layout.rows() - 1, _layout.columns(), _layout.rows());
		} else {
			_clearDirtyRows();
			_screen.clearDirtyCellsInRow(_layout.rows() - 1, _layout.columns(), _layout.rows());
		}
		_markDirtyRow(_layout.rows() - 1);
		return;
	}

	_setScrollRedrawRow(0);

	if (!_viewport.isFollowingTail()) {
		_setScrollRedrawRow(invalidRow);
	}
}

void LcdConsoleTerminalState::_markDirtyRow(int row) {
	if (_hooks.markDirtyRow != nullptr) {
		_hooks.markDirtyRow(_hooks.context, row);
	}
}

void LcdConsoleTerminalState::_markAllRowsDirty() {
	for (int row = 0; row < _layout.rows(); ++row) {
		_markDirtyRow(row);
	}
}

void LcdConsoleTerminalState::_clearDirtyRows() {
	if (_hooks.clearDirtyRows != nullptr) {
		_hooks.clearDirtyRows(_hooks.context);
	}
}

void LcdConsoleTerminalState::_setScrollRedrawRow(int row) {
	if (_hooks.setScrollRedrawRow != nullptr) {
		_hooks.setScrollRedrawRow(_hooks.context, row);
	}
}

void LcdConsoleTerminalState::_markCellDirty(int column, int row) {
	if (_screen.markDirtyCell(column, row, _layout.columns(), _layout.rows())) {
		_markDirtyRow(row);
	}
}

void LcdConsoleTerminalState::_invalidateView() {
	_setScrollRedrawRow(invalidRow);
	_clearDirtyRows();
	_markAllRowsDirty();
}

void LcdConsoleTerminalState::_followScrollbackTail() {
	LcdConsoleScrollbackNavigation::followTail(_viewport, _scrollback);
}

void LcdConsoleTerminalState::_followCursorColumn() {
	if (!_viewport.isFollowingTail()) {
		return;
	}
	if (_viewport.followCursorColumn(_cursorX, _layout.visibleColumns(), _layout.maxViewLeftColumn())) {
		_invalidateView();
	}
}

bool LcdConsoleTerminalState::_hardwareScrollEnabled() const {
	return _hooks.hardwareScrollEnabled != nullptr && _hooks.hardwareScrollEnabled(_hooks.context);
}

void LcdConsoleTerminalState::_flushDirtyRowsBeforeHardwareScroll(int rows) {
	if (_hooks.flushDirtyRowsBeforeHardwareScroll != nullptr) {
		_hooks.flushDirtyRowsBeforeHardwareScroll(_hooks.context, rows);
	}
}

bool LcdConsoleTerminalState::_advanceHardwareScroll(int pixels) {
	return _hooks.advanceHardwareScroll != nullptr && _hooks.advanceHardwareScroll(_hooks.context, pixels);
}

void LcdConsoleTerminalState::_shiftDirtyRowsAfterHardwareScroll(int rows) {
	if (_hooks.shiftDirtyRowsAfterHardwareScroll != nullptr) {
		_hooks.shiftDirtyRowsAfterHardwareScroll(_hooks.context, rows);
	}
}

} // namespace lexxos::app::picocalc_lcd_console::detail
