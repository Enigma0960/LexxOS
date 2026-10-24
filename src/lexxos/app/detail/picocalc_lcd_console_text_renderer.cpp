#include <lexxos/app/detail/picocalc_lcd_console_text_renderer.h>

#include <cstdint>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>
#include <lexxos/app/detail/picocalc_lcd_console_font.h>
#include <lexxos/app/detail/picocalc_lcd_console_hardware_scroll.h>
#include <lexxos/app/detail/picocalc_lcd_console_screen_buffer.h>
#include <lexxos/app/detail/picocalc_lcd_console_scrollback.h>
#include <lexxos/app/detail/picocalc_lcd_console_viewport.h>
#include <lexxos/devices/display_device.h>

namespace lexxos::app::picocalc_lcd_console::detail {

namespace {
#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH

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

	void storeRgb888(std::uint8_t* output, std::uint16_t color) {
		output[0] = rgb565Red8(color);
		output[1] = rgb565Green8(color);
		output[2] = rgb565Blue8(color);
	}
#endif
} // namespace

void LcdConsoleTextRenderer::drawRow(LcdConsoleTextRenderFrame const& frame, int row) const {
	if (!_isValid(frame) || row < 0 || row >= frame.rows) {
		return;
	}

	auto const rect = lexxos::devices::Rect{
	    .x = 0,
	    .y = frame.hardwareScroll->rowTopPixel(row),
	    .width = frame.visibleColumns * cellWidth,
	    .height = cellHeight,
	};

#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
	_cacheRowGlyphs(frame, row);
	auto* const rgb888Pixels = _rowRgb888Pixels.data();
	if (frame.display->beginRgb565Stream(rect)) {
		bool directRgb888 = true;
		for (int y = 0; y < cellHeight; ++y) {
			_fillCachedRowRgb888Pixels(frame, row, y, rgb888Pixels);
			if (!frame.display->writeRgb888Stream(rgb888Pixels, rect.width)) {
				directRgb888 = false;
				break;
			}
		}
		frame.display->endRgb565Stream();
		if (directRgb888) {
			frame.screen->clearDirtyCellsInRow(row, frame.columns, frame.rows);
			return;
		}
	}

	auto* const pixels = _rowRgb565Pixels.data();
#else
	std::uint16_t pixels[rowPixelCount]{};
#endif
	if (frame.display->beginRgb565Stream(rect)) {
		for (int y = 0; y < cellHeight; ++y) {
#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
			_fillCachedRowPixels(frame, row, y, pixels);
#else
			_fillRowPixels(frame, row, y, pixels);
#endif
			frame.display->writeRgb565Stream(pixels, rect.width);
		}
		frame.display->endRgb565Stream();
		frame.screen->clearDirtyCellsInRow(row, frame.columns, frame.rows);
		return;
	}

	for (int y = 0; y < cellHeight; ++y) {
		_fillRowPixels(frame, row, y, pixels);
		frame.display->blitRgb565({
		                              .x = 0,
		                              .y = frame.hardwareScroll->rowTopPixel(row) + y,
		                              .width = frame.visibleColumns * cellWidth,
		                              .height = 1,
		                          },
		    pixels, frame.visibleColumns * cellWidth);
	}
	frame.screen->clearDirtyCellsInRow(row, frame.columns, frame.rows);
}

void LcdConsoleTextRenderer::drawCell(LcdConsoleTextRenderFrame const& frame, int column, int row) const {
	if (!_isValid(frame) || column < 0 || row < 0 || column >= frame.visibleColumns || row >= frame.rows) {
		return;
	}

	auto const rect = lexxos::devices::Rect{
	    .x = column * cellWidth,
	    .y = frame.hardwareScroll->rowTopPixel(row),
	    .width = cellWidth,
	    .height = cellHeight,
	};

#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
	auto* const rgb888Pixels = _cellRgb888Pixels.data();
	for (int sourceY = 0; sourceY < cellHeight; ++sourceY) {
		_fillCellRowRgb888Pixels(frame, column, row, sourceY, rgb888Pixels + sourceY * cellWidth * rgb888BytesPerPixel);
	}
	if (frame.display->beginRgb565Stream(rect)) {
		bool const directRgb888 = frame.display->writeRgb888Stream(rgb888Pixels, cellWidth * cellHeight);
		frame.display->endRgb565Stream();
		if (directRgb888) {
			return;
		}
	}

	auto* const pixels = _cellRgb565Pixels.data();
#else
	std::uint16_t pixels[cellWidth * cellHeight]{};
#endif
	for (int sourceY = 0; sourceY < cellHeight; ++sourceY) {
		_fillCellRowPixels(frame, column, row, sourceY, pixels + sourceY * cellWidth);
	}
	// frame.display->blitRgb565({
	//                               .x = column * cellWidth,
	//                               .y = frame.hardwareScroll->rowTopPixel(row),
	//                               .width = cellWidth,
	//                               .height = cellHeight,
	//                           },
	frame.display->blitRgb565(rect, pixels, cellWidth);
}

bool LcdConsoleTextRenderer::_isValid(LcdConsoleTextRenderFrame const& frame) {
	return frame.display != nullptr && frame.screen != nullptr && frame.scrollback != nullptr && frame.viewport != nullptr && frame.hardwareScroll != nullptr && frame.columns > 0 && frame.visibleColumns > 0 && frame.rows > 0;
}

int LcdConsoleTextRenderer::_maxViewTopLine(LcdConsoleTextRenderFrame const& frame) {
	return static_cast<int>(frame.scrollback->count());
}

int LcdConsoleTextRenderer::_maxViewLeftColumn(LcdConsoleTextRenderFrame const& frame) {
	return frame.columns > frame.visibleColumns ? frame.columns - frame.visibleColumns : 0;
}

int LcdConsoleTextRenderer::_viewLeftColumn(LcdConsoleTextRenderFrame const& frame) {
	return frame.viewport->left(_maxViewLeftColumn(frame));
}

char LcdConsoleTextRenderer::_visibleCell(LcdConsoleTextRenderFrame const& frame, int column, int row) {
	if (column < 0 || row < 0 || column >= frame.visibleColumns || row >= frame.rows) {
		return blankCharacter;
	}
	auto const logicalColumn = _viewLeftColumn(frame) + column;
	if (logicalColumn < 0 || logicalColumn >= frame.columns) {
		return blankCharacter;
	}
	if (frame.viewport->isFollowingTail()) {
		return frame.screen->cell(logicalColumn, row);
	}

	auto const line = frame.viewport->top(_maxViewTopLine(frame)) + row;
	auto const historyLines = static_cast<int>(frame.scrollback->count());
	if (line < historyLines) {
		return frame.scrollback->cell(line, logicalColumn, frame.columns);
	}

	auto const liveRow = line - historyLines;
	if (liveRow >= 0 && liveRow < frame.rows) {
		return frame.screen->cell(logicalColumn, liveRow);
	}
	return blankCharacter;
}

std::uint8_t LcdConsoleTextRenderer::_visibleCellAttribute(LcdConsoleTextRenderFrame const& frame, int column, int row) {
	if (column < 0 || row < 0 || column >= frame.visibleColumns || row >= frame.rows) {
		return defaultCellAttribute;
	}
	auto const logicalColumn = _viewLeftColumn(frame) + column;
	if (logicalColumn < 0 || logicalColumn >= frame.columns) {
		return defaultCellAttribute;
	}
	if (frame.viewport->isFollowingTail()) {
		return frame.screen->attribute(logicalColumn, row);
	}

	auto const line = frame.viewport->top(_maxViewTopLine(frame)) + row;
	auto const historyLines = static_cast<int>(frame.scrollback->count());
	if (line < historyLines) {
		return defaultCellAttribute;
	}

	auto const liveRow = line - historyLines;
	if (liveRow >= 0 && liveRow < frame.rows) {
		return frame.screen->attribute(logicalColumn, liveRow);
	}
	return defaultCellAttribute;
}

std::uint16_t LcdConsoleTextRenderer::_paletteColor(std::uint8_t paletteIndex) {
	switch (paletteIndex & 0x0FU) {
		case paletteBlack:
			return ansiBlackColor;
		case paletteRed:
			return ansiRedColor;
		case paletteGreen:
			return ansiGreenColor;
		case paletteYellow:
			return ansiYellowColor;
		case paletteBlue:
			return ansiBlueColor;
		case paletteMagenta:
			return ansiMagentaColor;
		case paletteCyan:
			return ansiCyanColor;
		case paletteWhite:
			return ansiWhiteColor;
		case paletteBrightBlack:
			return ansiBrightBlackColor;
		case paletteBrightRed:
			return ansiBrightRedColor;
		case paletteBrightGreen:
			return ansiBrightGreenColor;
		case paletteBrightYellow:
			return ansiBrightYellowColor;
		case paletteBrightBlue:
			return ansiBrightBlueColor;
		case paletteBrightMagenta:
			return ansiBrightMagentaColor;
		case paletteBrightCyan:
			return ansiBrightCyanColor;
		case paletteBrightWhite:
			return ansiBrightWhiteColor;
		default:
			return foregroundColor;
	}
}

std::uint16_t LcdConsoleTextRenderer::_foregroundColor(std::uint8_t attribute) {
	return _paletteColor(attribute & 0x0FU);
}

std::uint16_t LcdConsoleTextRenderer::_backgroundColor(std::uint8_t attribute) {
	return _paletteColor((attribute >> 4U) & 0x0FU);
}

bool LcdConsoleTextRenderer::_isCursorVisibleAt(LcdConsoleTextRenderFrame const& frame, int column, int row) {
	return frame.drawCursor && frame.cursorVisible && frame.viewport->isFollowingTail() && frame.textMode && (_viewLeftColumn(frame) + column) == frame.cursorX && row == frame.cursorY;
}

void LcdConsoleTextRenderer::_fillCellRowPixels(LcdConsoleTextRenderFrame const& frame, int column, int row, int sourceY, std::uint16_t* output) {
	std::uint8_t glyph[glyphHeight]{};
	glyphRows(_visibleCell(frame, column, row), glyph);
	auto const attribute = _visibleCellAttribute(frame, column, row);
	auto const foreground = _foregroundColor(attribute);
	auto const background = _backgroundColor(attribute);
	_fillGlyphRowPixels(glyph, sourceY, foreground, background, output);
	if (_isCursorVisibleAt(frame, column, row)) {
		_invertCellRowPixels(output, foreground, background);
	}
}

void LcdConsoleTextRenderer::_fillGlyphRowPixels(
    std::uint8_t const (&glyph)[glyphHeight],
    int sourceY,
    std::uint16_t foreground,
    std::uint16_t background,
    std::uint16_t* output) {
	for (int sourceX = 0; sourceX < cellWidth; ++sourceX) {
		bool const active = sourceX < glyphWidth && sourceY < glyphHeight && ((glyph[sourceY] >> (glyphWidth - 1 - sourceX)) & 0x01U) != 0;
		output[sourceX] = active ? foreground : background;
	}
}

void LcdConsoleTextRenderer::_fillRowPixels(LcdConsoleTextRenderFrame const& frame, int row, int sourceY, std::uint16_t* output) {
	for (int x = 0; x < frame.visibleColumns; ++x) {
		_fillCellRowPixels(frame, x, row, sourceY, output + x * cellWidth);
	}
}

#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
void LcdConsoleTextRenderer::_cacheRowGlyphs(LcdConsoleTextRenderFrame const& frame, int row) const {
	for (int column = 0; column < frame.visibleColumns && column < maxVisibleColumns; ++column) {
		std::uint8_t glyph[glyphHeight]{};
		glyphRows(_visibleCell(frame, column, row), glyph);
		auto* const cached = _rowGlyphCache.data() + column * glyphHeight;
		for (int index = 0; index < glyphHeight; ++index) {
			cached[index] = glyph[index];
		}
		_rowAttributeCache[static_cast<std::size_t>(column)] = _visibleCellAttribute(frame, column, row);
	}
}

std::uint8_t const* LcdConsoleTextRenderer::_cachedGlyph(int column) const {
	return _rowGlyphCache.data() + column * glyphHeight;
}

std::uint8_t LcdConsoleTextRenderer::_cachedAttribute(int column) const {
	return _rowAttributeCache[static_cast<std::size_t>(column)];
}

void LcdConsoleTextRenderer::_fillGlyphRowPixels(
    std::uint8_t const* glyph,
    int sourceY,
    std::uint16_t foreground,
    std::uint16_t background,
    std::uint16_t* output) {
	for (int sourceX = 0; sourceX < cellWidth; ++sourceX) {
		bool const active = sourceX < glyphWidth && sourceY < glyphHeight && ((glyph[sourceY] >> (glyphWidth - 1 - sourceX)) & 0x01U) != 0;
		output[sourceX] = active ? foreground : background;
	}
}

void LcdConsoleTextRenderer::_fillCachedRowPixels(LcdConsoleTextRenderFrame const& frame, int row, int sourceY, std::uint16_t* output) const {
	for (int column = 0; column < frame.visibleColumns; ++column) {
		auto const attribute = _cachedAttribute(column);
		auto const foreground = _foregroundColor(attribute);
		auto const background = _backgroundColor(attribute);
		_fillGlyphRowPixels(_cachedGlyph(column), sourceY, foreground, background, output + column * cellWidth);
		if (_isCursorVisibleAt(frame, column, row)) {
			_invertCellRowPixels(output + column * cellWidth, foreground, background);
		}
	}
}

void LcdConsoleTextRenderer::_fillCellRowRgb888Pixels(LcdConsoleTextRenderFrame const& frame, int column, int row, int sourceY, std::uint8_t* output) {
	std::uint8_t glyph[glyphHeight]{};
	glyphRows(_visibleCell(frame, column, row), glyph);
	auto const attribute = _visibleCellAttribute(frame, column, row);
	_fillGlyphRowRgb888Pixels(
	    glyph,
	    sourceY,
	    _foregroundColor(attribute),
	    _backgroundColor(attribute),
	    _isCursorVisibleAt(frame, column, row),
	    output);
}

void LcdConsoleTextRenderer::_fillGlyphRowRgb888Pixels(
    std::uint8_t const (&glyph)[glyphHeight],
    int sourceY,
    std::uint16_t foreground,
    std::uint16_t background,
    bool invert,
    std::uint8_t* output) {
	_fillGlyphRowRgb888Pixels(static_cast<std::uint8_t const*>(glyph), sourceY, foreground, background, invert, output);
}

void LcdConsoleTextRenderer::_fillGlyphRowRgb888Pixels(
    std::uint8_t const* glyph,
    int sourceY,
    std::uint16_t foreground,
    std::uint16_t background,
    bool invert,
    std::uint8_t* output) {
	for (int sourceX = 0; sourceX < cellWidth; ++sourceX) {
		bool const active = sourceX < glyphWidth && sourceY < glyphHeight && ((glyph[sourceY] >> (glyphWidth - 1 - sourceX)) & 0x01U) != 0;
		auto const color = active
		    ? (invert ? background : foreground)
		    : (invert ? foreground : background);
		storeRgb888(output + sourceX * rgb888BytesPerPixel, color);
	}
}

void LcdConsoleTextRenderer::_fillRowRgb888Pixels(LcdConsoleTextRenderFrame const& frame, int row, int sourceY, std::uint8_t* output) {
	for (int x = 0; x < frame.visibleColumns; ++x) {
		_fillCellRowRgb888Pixels(frame, x, row, sourceY, output + x * cellWidth * rgb888BytesPerPixel);
	}
}

void LcdConsoleTextRenderer::_fillCachedRowRgb888Pixels(LcdConsoleTextRenderFrame const& frame, int row, int sourceY, std::uint8_t* output) const {
	for (int column = 0; column < frame.visibleColumns; ++column) {
		auto const attribute = _cachedAttribute(column);
		_fillGlyphRowRgb888Pixels(
		    _cachedGlyph(column),
		    sourceY,
		    _foregroundColor(attribute),
		    _backgroundColor(attribute),
		    _isCursorVisibleAt(frame, column, row),
		    output + column * cellWidth * rgb888BytesPerPixel);
	}
}
#endif

void LcdConsoleTextRenderer::_invertCellRowPixels(std::uint16_t* output, std::uint16_t foreground, std::uint16_t background) {
	for (int x = 0; x < cellWidth; ++x) {
		output[x] = output[x] == foreground ? background : foreground;
	}
}

} // namespace lexxos::app::picocalc_lcd_console::detail
