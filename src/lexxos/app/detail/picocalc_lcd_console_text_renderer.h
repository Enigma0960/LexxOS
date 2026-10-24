#pragma once

#include <cstdint>

#include <lexxos/app/detail/picocalc_lcd_console_config.h>

#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
	#include <array>
#endif

namespace lexxos::devices {
class DisplayDevice;
} // namespace lexxos::devices

namespace lexxos::app::picocalc_lcd_console::detail {

class LcdConsoleHardwareScroll;
class LcdConsoleScreenBuffer;
class LcdConsoleScrollback;
class LcdConsoleViewport;

struct LcdConsoleTextRenderFrame {
	lexxos::devices::DisplayDevice* display{nullptr};
	LcdConsoleScreenBuffer* screen{nullptr};
	LcdConsoleScrollback const* scrollback{nullptr};
	LcdConsoleViewport const* viewport{nullptr};
	LcdConsoleHardwareScroll const* hardwareScroll{nullptr};
	int columns{0};
	int visibleColumns{0};
	int rows{0};
	int cursorX{0};
	int cursorY{0};
	bool drawCursor{false};
	bool cursorVisible{false};
	bool textMode{false};
};

class LcdConsoleTextRenderer final {
public:
	void drawRow(LcdConsoleTextRenderFrame const& frame, int row) const;
	void drawCell(LcdConsoleTextRenderFrame const& frame, int column, int row) const;

private:
#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
	static constexpr int rgb888BytesPerPixel = 3;
	mutable std::array<std::uint8_t, rowPixelCount * rgb888BytesPerPixel> _rowRgb888Pixels{};
	mutable std::array<std::uint16_t, rowPixelCount> _rowRgb565Pixels{};
	mutable std::array<std::uint8_t, cellWidth * cellHeight * rgb888BytesPerPixel> _cellRgb888Pixels{};
	mutable std::array<std::uint16_t, cellWidth * cellHeight> _cellRgb565Pixels{};
	mutable std::array<std::uint8_t, maxVisibleColumns * glyphHeight> _rowGlyphCache{};
	mutable std::array<std::uint8_t, maxVisibleColumns> _rowAttributeCache{};
#endif

	[[nodiscard]] static bool _isValid(LcdConsoleTextRenderFrame const& frame);
	[[nodiscard]] static int _maxViewTopLine(LcdConsoleTextRenderFrame const& frame);
	[[nodiscard]] static int _maxViewLeftColumn(LcdConsoleTextRenderFrame const& frame);
	[[nodiscard]] static int _viewLeftColumn(LcdConsoleTextRenderFrame const& frame);
	[[nodiscard]] static char _visibleCell(LcdConsoleTextRenderFrame const& frame, int column, int row);
	[[nodiscard]] static std::uint8_t _visibleCellAttribute(LcdConsoleTextRenderFrame const& frame, int column, int row);
	[[nodiscard]] static std::uint16_t _paletteColor(std::uint8_t paletteIndex);
	[[nodiscard]] static std::uint16_t _foregroundColor(std::uint8_t attribute);
	[[nodiscard]] static std::uint16_t _backgroundColor(std::uint8_t attribute);
	[[nodiscard]] static bool _isCursorVisibleAt(LcdConsoleTextRenderFrame const& frame, int column, int row);
	static void _fillCellRowPixels(LcdConsoleTextRenderFrame const& frame, int column, int row, int sourceY, std::uint16_t* output);
	static void _fillGlyphRowPixels(
	    std::uint8_t const (&glyph)[glyphHeight],
	    int sourceY,
	    std::uint16_t foreground,
	    std::uint16_t background,
	    std::uint16_t* output);
	static void _fillRowPixels(LcdConsoleTextRenderFrame const& frame, int row, int sourceY, std::uint16_t* output);
#if LEXXOS_PICOCALC_LCD_RGB888_FAST_PATH
	void _cacheRowGlyphs(LcdConsoleTextRenderFrame const& frame, int row) const;
	[[nodiscard]] std::uint8_t const* _cachedGlyph(int column) const;
	[[nodiscard]] std::uint8_t _cachedAttribute(int column) const;
	static void _fillGlyphRowPixels(
	    std::uint8_t const* glyph,
	    int sourceY,
	    std::uint16_t foreground,
	    std::uint16_t background,
	    std::uint16_t* output);
	void _fillCachedRowPixels(LcdConsoleTextRenderFrame const& frame, int row, int sourceY, std::uint16_t* output) const;
	static void _fillCellRowRgb888Pixels(LcdConsoleTextRenderFrame const& frame, int column, int row, int sourceY, std::uint8_t* output);
	static void _fillGlyphRowRgb888Pixels(
	    std::uint8_t const* glyph,
	    int sourceY,
	    std::uint16_t foreground,
	    std::uint16_t background,
	    bool invert,
	    std::uint8_t* output);
	static void _fillGlyphRowRgb888Pixels(
	    std::uint8_t const (&glyph)[glyphHeight],
	    int sourceY,
	    std::uint16_t foreground,
	    std::uint16_t background,
	    bool invert,
	    std::uint8_t* output);
	static void _fillRowRgb888Pixels(LcdConsoleTextRenderFrame const& frame, int row, int sourceY, std::uint8_t* output);
	void _fillCachedRowRgb888Pixels(LcdConsoleTextRenderFrame const& frame, int row, int sourceY, std::uint8_t* output) const;
#endif
	static void _invertCellRowPixels(std::uint16_t* output, std::uint16_t foreground, std::uint16_t background);
};

} // namespace lexxos::app::picocalc_lcd_console::detail
