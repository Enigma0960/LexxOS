#include <lexxos/app/detail/picocalc_lcd_console_renderer.h>

#include <array>
#include <cstdint>

namespace {

std::uint8_t rgb565Red8(std::uint16_t value) {
	auto const red = static_cast<std::uint8_t>((value >> 11U) & 0x1FU);
	return static_cast<std::uint8_t>((red << 3U) | (red >> 2U));
}

std::uint8_t rgb565Green8(std::uint16_t value) {
	auto const green = static_cast<std::uint8_t>((value >> 5U) & 0x3FU);
	return static_cast<std::uint8_t>((green << 2U) | (green >> 4U));
}

std::uint8_t rgb565Blue8(std::uint16_t value) {
	auto const blue = static_cast<std::uint8_t>(value & 0x1FU);
	return static_cast<std::uint8_t>((blue << 3U) | (blue >> 2U));
}

void fillRgb888Solid(std::uint8_t* output, int pixels, std::uint16_t color) {
	auto const red = rgb565Red8(color);
	auto const green = rgb565Green8(color);
	auto const blue = rgb565Blue8(color);
	for (int index = 0; index < pixels; ++index) {
		auto* const pixel = output + index * 3;
		pixel[0] = red;
		pixel[1] = green;
		pixel[2] = blue;
	}
}

} // namespace

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleRenderer::initialize(lexxos::os::OperatingSystem& operatingSystem) {
	_operatingSystem = &operatingSystem;
	_display = operatingSystem.devices().displayDevice(consoleDisplayName);
	if (_display == nullptr) {
		_display = operatingSystem.devices().primaryDisplayDevice();
	}

	_ready = false;
	if (_display == nullptr || !_display->isReady()) {
		return;
	}

	if (!_layout.configure(*_display)) {
		return;
	}

	_terminal.setRenderHooks(_terminalHooks());
	_terminal.reset(_layout);
	_markAllRowsDirty();
	_queue.reset();
	_scrollRedrawRow = invalidRow;
	_hardwareScroll.resetState(_layout.rows());
	_statusBar.markDirty();
	_clearDirtyRows();

	auto lock = _lockDisplay();
	if (!lock) {
		return;
	}

	if (!_display->clear(backgroundColor)) {
		return;
	}
	// _hardwareScroll.configure(_display, _layout.rows());

	(void) _ensureHardwareScrollConfigured();
	if (_display->beginFrame()) {
		_drawStatusBar();
		_display->endFrame();
		(void) _statusBar.takeDirty();
	}
	_ready = true;
}

void LcdConsoleRenderer::write(std::string_view text) {
	if (!_ready || text.empty() || _display == nullptr || _operatingSystem == nullptr) {
		return;
	}
	if (!_textMode) {
		return;
	}

	for (char const value : text) {
		_lockState();
		_queue.enqueue(value);
		_unlockState();
	}
}

void LcdConsoleRenderer::setStatusBarInfo(lexxos::app::picocalc_lcd_console::StatusBarInfo const& info) {
	_lockState();
	_statusBar.set(info);
	_unlockState();
}

void LcdConsoleRenderer::refreshStatusBar() {
	_lockState();
	_statusBar.markDirty();
	_unlockState();
}

void LcdConsoleRenderer::flush() {
	if (!_ready || !_textMode || _display == nullptr || _operatingSystem == nullptr) {
		return;
	}

	auto lock = _lockDisplay();
	if (!lock) {
		++_stats.displayLockFailures;
		return;
	}

	// A previous failed vertical scroll command leaves the renderer on the slow
	// full-screen software scroll path. Re-arm hardware scroll before processing
	// queued text so newline-heavy output can still use the controller scroll.
	(void) _ensureHardwareScrollConfigured();

	++_stats.beginFrameCalls;
	if (!_display->beginFrame()) {
		++_stats.beginFrameFailures;
		return;
	}
	++_stats.flushCalls;

	bool didWork = false;
	bool processedCharacters = false;
	auto const drawCursor = _drawCursor;
	_lockState();
	bool const statusBarDirty = _statusBar.takeDirty();
	_unlockState();
	if (statusBarDirty) {
		_drawStatusBar();
		didWork = true;
	}

	for (int index = 0; index < maxCharactersPerFlush; ++index) {
		char value{};
		_lockState();
		bool const hasValue = _textMode && _queue.dequeue(value);
		_unlockState();
		if (!hasValue) {
			break;
		}

		if (!processedCharacters) {
			_terminal.markCursorDirty(_textMode);
			_drawCursor = false;
			processedCharacters = true;
		}

		++_stats.charactersProcessed;
		_terminal.writeCharacterWithoutCursor(value);
		didWork = true;
	}

	_lockState();
	bool const queueEmpty = !_queue.hasCharacters();
	_unlockState();
	if (processedCharacters && queueEmpty) {
		_drawCursor = drawCursor;
		_terminal.markCursorDirty(_textMode);
	}

	if (_scrollRedrawRow != invalidRow) {
		didWork = _flushPendingScroll(maxScrollRowsPerFlush) || didWork;
	} else {
		didWork = _drawDirtyRows(maxDirtyRowsPerFlush) || didWork;
	}

	(void) didWork;

	_display->endFrame();
	_drawCursor = drawCursor;
}

bool LcdConsoleRenderer::update() {
	flush();
	return isReady();
}

void LcdConsoleRenderer::setTextModeEnabled(bool enabled) {
	bool resetScroll = false;
	_lockState();
	_textMode = enabled;
	if (!enabled) {
		_queue.reset();
		_scrollRedrawRow = invalidRow;
		_clearDirtyRowsUnlocked();
		resetScroll = true;
	} else {
		_terminal.onTextModeEnabled();
		resetScroll = true;
	}
	_unlockState();
	// _hardwareScroll.reset(_display, resetScroll);

	if (enabled && resetScroll) {
		_hardwareScroll.resetState(_layout.rows());
		(void) _ensureHardwareScrollConfigured();
	} else {
		_hardwareScroll.reset(_display, resetScroll);
	}
}

void LcdConsoleRenderer::clear() {
	if (!_ready || _display == nullptr || _operatingSystem == nullptr) {
		return;
	}

	++_stats.clearCalls;

	_lockState();
	_queue.reset();
	_scrollRedrawRow = invalidRow;
	_terminal.clear();
	_unlockState();

	bool clearedOnDisplay = false;
	std::uint32_t clearedPixels = 0;
	auto lock = _lockDisplay();
	if (lock) {
		++_stats.beginFrameCalls;
		if (_display->beginFrame()) {
			// A shell clear is an explicit full-screen text reset.  Clear every
			// physical pixel below the status bar so old glyph tails outside the
			// logical character grid cannot remain visible after hardware scroll.
			_hardwareScroll.configure(_display, _layout.rows());
			clearedOnDisplay = _clearTextAreaBackground();
			if (clearedOnDisplay) {
				clearedPixels = _textAreaPixelCount();
			}
			_display->endFrame();
		} else {
			++_stats.beginFrameFailures;
		}
	} else {
		++_stats.displayLockFailures;
	}

	if (clearedOnDisplay) {
		++_stats.clearTextAreaCalls;
		_stats.clearTextAreaPixels += clearedPixels;
	} else {
		++_stats.clearTextAreaFailures;
	}

	if (!clearedOnDisplay) {
		return;
	}

	_lockState();
	// The physical text area is already cleared. Do not repaint all terminal
	// rows over several mirror ticks; keep only the cursor cell dirty so the next
	// flush can restore the cursor/prompt position cheaply.
	_clearDirtyRowsUnlocked();
	_terminal.screen().clearDirtyCells();
	_terminal.markCursorDirty(_textMode);
	_unlockState();
}

void LcdConsoleRenderer::refresh() {
	if (!_ready || _display == nullptr || _operatingSystem == nullptr) {
		return;
	}

	_lockState();
	bool const statusBarDirty = _statusBar.takeDirty();
	_unlockState();
	if (!statusBarDirty && !_terminal.screen().hasDirtyCells(_layout.columns(), _layout.rows())) {
		return;
	}

	auto lock = _lockDisplay();
	if (!lock) {
		++_stats.displayLockFailures;
		return;
	}

	++_stats.beginFrameCalls;
	if (!_display->beginFrame()) {
		++_stats.beginFrameFailures;
		return;
	}
	++_stats.refreshCalls;

	if (statusBarDirty) {
		_drawStatusBar();
	}

	if (_scrollRedrawRow != invalidRow) {
		while (_scrollRedrawRow != invalidRow) {
			_flushPendingScroll(_layout.rows());
		}
	} else if (!_drawDirtyRows(_layout.rows())) {
		int drawn = 0;
		auto const leftColumn = _terminal.viewLeftColumn();
		for (int y = 0; y < _layout.rows(); ++y) {
			for (int x = 0; x < _layout.visibleColumns(); ++x) {
				auto const logicalColumn = leftColumn + x;
				if (!_terminal.screen().dirtyCell(logicalColumn, y)) {
					continue;
				}
				_drawCell(x, y);
				_terminal.screen().dirtyCell(logicalColumn, y) = false;
				++drawn;
				if (drawn >= maxDirtyCellsPerRefresh) {
					_display->endFrame();
					return;
				}
			}
		}
	}
	_display->endFrame();
}

bool LcdConsoleRenderer::isReady() const {
	return _ready;
}

void LcdConsoleRenderer::scrollbackUp(int lines) {
	if (lines <= 0) {
		return;
	}
	_lockState();
	auto const change = _terminal.moveScrollbackView(-lines);
	_applyViewChange(change);
	_unlockState();
	_hardwareScroll.reset(_display, change.resetHardwareScroll);
}

void LcdConsoleRenderer::scrollbackDown(int lines) {
	if (lines <= 0) {
		return;
	}
	_lockState();
	auto const change = _terminal.moveScrollbackView(lines);
	_applyViewChange(change);
	_unlockState();
	_hardwareScroll.reset(_display, change.resetHardwareScroll);
}

void LcdConsoleRenderer::scrollbackHome() {
	_lockState();
	auto const change = _terminal.setScrollbackViewTop(0);
	_applyViewChange(change);
	_unlockState();
	_hardwareScroll.reset(_display, change.resetHardwareScroll);
}

void LcdConsoleRenderer::scrollbackEnd() {
	_lockState();
	auto const change = _terminal.setScrollbackViewTop(_terminal.maxViewTopLine());
	_applyViewChange(change);
	_unlockState();
	_hardwareScroll.reset(_display, change.resetHardwareScroll);
}

void LcdConsoleRenderer::scrollbackLeft(int columns) {
	if (columns <= 0) {
		return;
	}
	_lockState();
	auto const change = _terminal.moveHorizontalView(-columns);
	_applyViewChange(change);
	_unlockState();
	_hardwareScroll.reset(_display, change.resetHardwareScroll);
}

void LcdConsoleRenderer::scrollbackRight(int columns) {
	if (columns <= 0) {
		return;
	}
	_lockState();
	auto const change = _terminal.moveHorizontalView(columns);
	_applyViewChange(change);
	_unlockState();
	_hardwareScroll.reset(_display, change.resetHardwareScroll);
}

void LcdConsoleRenderer::scrollbackLeftEdge() {
	_lockState();
	auto const change = _terminal.setHorizontalViewLeft(0);
	_applyViewChange(change);
	_unlockState();
	_hardwareScroll.reset(_display, change.resetHardwareScroll);
}

void LcdConsoleRenderer::scrollbackRightEdge() {
	_lockState();
	auto const change = _terminal.setHorizontalViewLeft(_terminal.maxViewLeftColumn());
	_applyViewChange(change);
	_unlockState();
	_hardwareScroll.reset(_display, change.resetHardwareScroll);
}

void LcdConsoleRenderer::scrollbackResetView() {
	_lockState();
	auto const change = _terminal.resetView();
	_applyViewChange(change);
	_unlockState();
	_hardwareScroll.reset(_display, change.resetHardwareScroll);
}

bool LcdConsoleRenderer::isScrollbackActive() const {
	return _terminal.isScrollbackActive();
}

lexxos::app::picocalc_lcd_console::ConsoleInfo LcdConsoleRenderer::info() {
	lexxos::app::picocalc_lcd_console::ConsoleInfo output{};
	_lockState();
	output.ready = _ready;
	output.textMode = _textMode;
	output.columns = _layout.visibleColumns();
	output.rows = _layout.rows();
	output.logicalColumns = _layout.columns();
	output.viewLeftColumn = _terminal.viewLeftColumn();
	output.maxViewLeftColumn = _terminal.maxViewLeftColumn();
	output.cursorX = _terminal.cursorX();
	output.cursorY = _terminal.cursorY();
	output.queueCount = _queue.count();
	output.scrollbackCapacity = _terminal.scrollback().capacity();
	output.scrollbackCount = _terminal.scrollback().count();
	output.scrollbackDroppedRows = _terminal.scrollback().droppedRows();
	output.viewTopLine = _terminal.viewTopLine();
	output.viewFollowTail = _terminal.viewFollowTail();
	output.hardwareScrollEnabled = _hardwareScroll.enabled();
	output.scrollAreaPixels = _hardwareScroll.areaPixels();
	output.scrollStartPixels = _hardwareScroll.startPixels();
	output.drawCursor = _drawCursor;
	output.cursorVisible = _terminal.cursorVisible();

	output.statusBarPixels = statusBarHeightPixels;
	auto const statusBar = _statusBar.snapshot();
	output.statusBarVisible = statusBar.visible;
	output.statusBarBatteryValid = statusBar.batteryValid;
	output.statusBarBatteryPercent = statusBar.batteryPercent;
	output.statusBarBatteryCharging = statusBar.batteryCharging;
	output.statusBarTimeValid = statusBar.timeValid;
	output.statusBarHour = statusBar.hour;
	output.statusBarMinute = statusBar.minute;
	output.statusBarSecond = statusBar.second;
	output.statusBarWifiValid = statusBar.wifiValid;
	output.statusBarWifiConnected = statusBar.wifiConnected;
	output.statusBarWifiSignalLevel = statusBar.wifiSignalLevel;
	output.statusBarUpdatedMs = statusBar.updatedMs;

	_unlockState();
	return output;
}

lexxos::app::picocalc_lcd_console::ConsoleRenderStats LcdConsoleRenderer::renderStats() {
	_lockState();
	auto const output = _stats;
	_unlockState();
	return output;
}

void LcdConsoleRenderer::resetRenderStats() {
	_lockState();
	_stats = {};
	_unlockState();
}

lexxos::devices::DeviceResourceLock LcdConsoleRenderer::_lockDisplay() const {
	return _operatingSystem->devices().lockDisplayDevice(
	    _display->name(),
	    lexxos::devices::DeviceResourcePriority::Background,
	    consoleOwner);
}

LcdConsoleTerminalRenderHooks LcdConsoleRenderer::_terminalHooks() {
	return {
	    .context = this,
	    .markDirtyRow = _terminalMarkDirtyRow,
	    .clearDirtyRows = _terminalClearDirtyRows,
	    .setScrollRedrawRow = _terminalSetScrollRedrawRow,
	    .hardwareScrollEnabled = _terminalHardwareScrollEnabled,
	    .flushDirtyRowsBeforeHardwareScroll = _terminalFlushDirtyRowsBeforeHardwareScroll,
	    .advanceHardwareScroll = _terminalAdvanceHardwareScroll,
	    .shiftDirtyRowsAfterHardwareScroll = _terminalShiftDirtyRowsAfterHardwareScroll,
	};
}

void LcdConsoleRenderer::_terminalMarkDirtyRow(void* context, int row) {
	static_cast<LcdConsoleRenderer*>(context)->_markDirtyRow(row);
}

void LcdConsoleRenderer::_terminalClearDirtyRows(void* context) {
	static_cast<LcdConsoleRenderer*>(context)->_clearDirtyRowsUnlocked();
}

void LcdConsoleRenderer::_terminalSetScrollRedrawRow(void* context, int row) {
	static_cast<LcdConsoleRenderer*>(context)->_scrollRedrawRow = row;
}

bool LcdConsoleRenderer::_terminalHardwareScrollEnabled(void* context) {
	return static_cast<LcdConsoleRenderer*>(context)->_hardwareScroll.enabled();
}

void LcdConsoleRenderer::_terminalFlushDirtyRowsBeforeHardwareScroll(void* context, int rows) {
	auto& renderer = *static_cast<LcdConsoleRenderer*>(context);
	++renderer._stats.hardwareScrollPreFlushCalls;
	auto const drawCursor = renderer._drawCursor;
	renderer._drawCursor = false;
	(void) renderer._drawDirtyRows(rows);
	renderer._drawCursor = drawCursor;
}

bool LcdConsoleRenderer::_terminalAdvanceHardwareScroll(void* context, int pixels) {
	auto& renderer = *static_cast<LcdConsoleRenderer*>(context);
	auto const advanced = renderer._hardwareScroll.advance(renderer._display, pixels);
	if (advanced) {
		++renderer._stats.hardwareScrollAdvanceCalls;
		renderer._stats.hardwareScrollPixels += static_cast<std::uint32_t>(pixels > 0 ? pixels : 0);
	}
	return advanced;
}

void LcdConsoleRenderer::_terminalShiftDirtyRowsAfterHardwareScroll(void* context, int rows) {
	auto& renderer = *static_cast<LcdConsoleRenderer*>(context);
	++renderer._stats.hardwareScrollDirtyShiftCalls;
	renderer._shiftDirtyRowsAfterHardwareScroll(rows);
}

void LcdConsoleRenderer::_applyViewChange(LcdConsoleScrollbackViewChange const& change) {
	if (!change.hideStatusBar) {
		return;
	}
	lexxos::app::picocalc_lcd_console::StatusBarInfo bootStatus{};
	bootStatus.visible = false;
	_statusBar.set(bootStatus);
}

bool LcdConsoleRenderer::_ensureHardwareScrollConfigured() {
	if (_display == nullptr || _hardwareScroll.enabled()) {
		return _hardwareScroll.enabled();
	}
	++_stats.hardwareScrollConfigureCalls;
	_hardwareScroll.configure(_display, _layout.rows());
	if (!_hardwareScroll.enabled()) {
		++_stats.hardwareScrollConfigureFailures;
	}
	return _hardwareScroll.enabled();
}

void LcdConsoleRenderer::_lockState() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	taskENTER_CRITICAL();
#else
	while (_stateLock.test_and_set(std::memory_order_acquire)) {
	}
#endif
}

void LcdConsoleRenderer::_unlockState() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	taskEXIT_CRITICAL();
#else
	_stateLock.clear(std::memory_order_release);
#endif
}

void LcdConsoleRenderer::_markDirtyRow(int row) {
	if (row >= 0 && row < _layout.rows()) {
		_dirtyRows[static_cast<std::size_t>(row)] = true;
		if (_scrollRedrawRow != invalidRow && row < _scrollRedrawRow) {
			_scrollRedrawRow = row;
		}
	}
}

void LcdConsoleRenderer::_markAllRowsDirty() {
	for (int row = 0; row < _layout.rows(); ++row) {
		_markDirtyRow(row);
	}
}

void LcdConsoleRenderer::_clearDirtyRows() {
	_lockState();
	_clearDirtyRowsUnlocked();
	_unlockState();
}

void LcdConsoleRenderer::_clearDirtyRowsUnlocked() {
	for (auto& dirty : _dirtyRows) {
		dirty = false;
	}
	_dirtyScanRow = 0;
}

bool LcdConsoleRenderer::_flushPendingScroll(int maxRows) {
	if (_scrollRedrawRow == invalidRow) {
		return false;
	}

	int rowsDrawn = 0;
	while (_scrollRedrawRow < _layout.rows() && rowsDrawn < maxRows) {
		auto const row = _scrollRedrawRow;
		_drawRow(row);
		_dirtyRows[static_cast<std::size_t>(row)] = false;
		++_scrollRedrawRow;
		++rowsDrawn;
	}

	if (_scrollRedrawRow >= _layout.rows()) {
		_scrollRedrawRow = invalidRow;
	}

	_stats.scrollRowsDrawn += static_cast<std::uint32_t>(rowsDrawn);
	return rowsDrawn != 0;
}

bool LcdConsoleRenderer::_drawDirtyRows(int maxRows) {
	int rowsDrawn = 0;
	for (int pass = 0; pass < _layout.rows() && rowsDrawn < maxRows; ++pass) {
		int const row = (_dirtyScanRow + pass) % _layout.rows();
		if (!_dirtyRows[static_cast<std::size_t>(row)]) {
			continue;
		}
		_drawRow(row);
		_dirtyRows[static_cast<std::size_t>(row)] = false;
		_dirtyScanRow = (row + 1) % _layout.rows();
		++rowsDrawn;
	}

	_stats.dirtyRowsDrawn += static_cast<std::uint32_t>(rowsDrawn);
	return rowsDrawn != 0;
}

void LcdConsoleRenderer::_shiftDirtyRowsAfterHardwareScroll(int rows) {
	if (rows <= 0) {
		return;
	}

	if (rows > _layout.rows()) {
		rows = _layout.rows();
	}

	if (rows > maxRows) {
		rows = maxRows;
	}

	for (int row = 1; row < rows; ++row) {
		_dirtyRows[static_cast<std::size_t>(row - 1)] = _dirtyRows[static_cast<std::size_t>(row)];
	}
	_dirtyRows[static_cast<std::size_t>(rows - 1)] = false;
	_dirtyScanRow = 0;

	if (_scrollRedrawRow != invalidRow) {
		_scrollRedrawRow = _scrollRedrawRow > 0 ? _scrollRedrawRow - 1 : 0;
	}
}

void LcdConsoleRenderer::_drawRow(int row) {
	++_stats.drawRowCalls;
	_textRenderer.drawRow(_textRenderFrame(), row);
}

void LcdConsoleRenderer::_drawCell(int column, int row) {
	++_stats.drawCellCalls;
	_textRenderer.drawCell(_textRenderFrame(), column, row);
}

LcdConsoleTextRenderFrame LcdConsoleRenderer::_textRenderFrame() {
	return LcdConsoleTextRenderFrame{
	    .display = _display,
	    .screen = &_terminal.screen(),
	    .scrollback = &_terminal.scrollback(),
	    .viewport = &_terminal.viewport(),
	    .hardwareScroll = &_hardwareScroll,
	    .columns = _layout.columns(),
	    .visibleColumns = _layout.visibleColumns(),
	    .rows = _layout.rows(),
	    .cursorX = _terminal.cursorX(),
	    .cursorY = _terminal.cursorY(),
	    .drawCursor = _drawCursor,
	    .cursorVisible = _terminal.cursorVisible(),
	    .textMode = _textMode,
	};
}

int LcdConsoleRenderer::_visibleRowsNeedingClear() const {
	if (_layout.rows() <= 0) {
		return 0;
	}

	int lastRow = _terminal.cursorY();
	if (lastRow < 0) {
		lastRow = 0;
	} else if (lastRow >= _layout.rows()) {
		lastRow = _layout.rows() - 1;
	}

	auto const& screen = _terminal.screen();
	for (int row = 0; row < _layout.rows(); ++row) {
		for (int column = 0; column < _layout.columns(); ++column) {
			if (screen.cell(column, row) != blankCharacter) {
				lastRow = row;
				break;
			}
		}
	}
	return lastRow + 1;
}

std::uint32_t LcdConsoleRenderer::_textAreaPixelCount() const {
	if (_display == nullptr) {
		return 0;
	}

	auto width = _display->width();
	auto const height = _display->height() - statusBarHeightPixels;
	if (width <= 0 || height <= 0) {
		return 0;
	}

	if (width > maxStatusBarPixelWidth) {
		width = maxStatusBarPixelWidth;
	}

	return static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height);
}

std::uint32_t LcdConsoleRenderer::_textRowsPixelCount(int rows) const {
	if (_display == nullptr || rows <= 0) {
		return 0;
	}
	if (rows > _layout.rows()) {
		rows = _layout.rows();
	}

	auto width = _display->width();
	if (width <= 0) {
		return 0;
	}
	if (width > maxStatusBarPixelWidth) {
		width = maxStatusBarPixelWidth;
	}

	return static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(rows * cellHeight);
}

bool LcdConsoleRenderer::_clearTextRowsBackground(int rows) {
	if (_display == nullptr || rows <= 0) {
		return false;
	}
	if (rows >= _layout.rows()) {
		return _clearTextAreaBackground();
	}

	auto width = _display->width();
	if (width <= 0) {
		return false;
	}
	if (width > maxStatusBarPixelWidth) {
		width = maxStatusBarPixelWidth;
	}

	for (int row = 0; row < rows; ++row) {
		auto const rect = lexxos::devices::Rect{
		    .x = 0,
		    .y = _hardwareScroll.rowTopPixel(row),
		    .width = width,
		    .height = cellHeight,
		};
		if (!_display->fillRgb565(rect, backgroundColor)) {
			return false;
		}
	}
	return true;
}

bool LcdConsoleRenderer::_clearTextAreaBackground() {
	if (_display == nullptr) {
		return false;
	}

	auto width = _display->width();
	auto const height = _display->height() - statusBarHeightPixels;
	if (width <= 0 || height <= 0) {
		return false;
	}
	if (width > maxStatusBarPixelWidth) {
		width = maxStatusBarPixelWidth;
	}

	auto const rect = lexxos::devices::Rect{
	    .x = 0,
	    .y = statusBarHeightPixels,
	    .width = width,
	    .height = height,
	};

	if (_display->fillRgb565(rect, backgroundColor)) {
		return true;
	}

#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
	std::array<std::uint8_t, maxStatusBarPixelWidth * 3> rgb888Row{};
	fillRgb888Solid(rgb888Row.data(), width, backgroundColor);
	if (_display->beginRgb565Stream(rect)) {
		bool ok = true;
		for (int y = 0; y < height; ++y) {
			if (!_display->writeRgb888Stream(rgb888Row.data(), width)) {
				ok = false;
				break;
			}
		}
		_display->endRgb565Stream();
		if (ok) {
			return true;
		}
	}
#endif

	std::array<std::uint16_t, maxStatusBarPixelWidth> row{};
	for (int index = 0; index < width; ++index) {
		row[static_cast<std::size_t>(index)] = backgroundColor;
	}
	if (!_display->beginRgb565Stream(rect)) {
		return false;
	}
	for (int y = 0; y < height; ++y) {
		if (!_display->writeRgb565Stream(row.data(), width)) {
			_display->endRgb565Stream();
			return false;
		}
	}
	return _display->endRgb565Stream();
}

void LcdConsoleRenderer::_drawStatusBar() {
	if (_display == nullptr) {
		return;
	}

	_lockState();
	auto const statusBar = _statusBar.snapshot();
	_unlockState();
	++_stats.statusBarDraws;
	_statusRenderer.draw(*_display, statusBar);
}

} // namespace lexxos::app::picocalc_lcd_console::detail
