#include <lexxos/tui/screen.h>

#include <lexxos/app/detail/picocalc_lcd_console_font.h>
#include <lexxos/devices/display_device.h>

namespace lexxos::tui {

namespace {

	void fillLine(std::uint16_t* pixels, int count, std::uint16_t color) {
		for (int index = 0; index < count; ++index) {
			pixels[index] = color;
		}
	}

} // namespace

Screen::Screen(lexxos::devices::DisplayDevice& display)
    : _display(display) {
}

int Screen::width() const {
	return _display.width();
}

int Screen::height() const {
	return _display.height();
}

int Screen::columns() const {
	return width() / cellWidth;
}

int Screen::rows() const {
	return height() / cellHeight;
}

bool Screen::beginFrame() {
	return _display.beginFrame();
}

bool Screen::endFrame() {
	return _display.endFrame();
}

bool Screen::clear(std::uint16_t color) {
	auto const full = lexxos::devices::Rect{.x = 0, .y = 0, .width = width(), .height = height()};
	if (_display.fillRgb565(full, color)) {
		return true;
	}

	if (width() <= 0 || width() > maxLinePixels) {
		return false;
	}

	fillLine(_line.data(), width(), color);
	for (int y = 0; y < height(); ++y) {
		if (!_display.blitRgb565({.x = 0, .y = y, .width = width(), .height = 1}, _line.data(), width())) {
			return false;
		}
	}

	return true;
}

bool Screen::drawText(int column, int row, std::string_view text, TextStyle style, bool fillToEnd) {
	if (column < 0 || row < 0 || column >= columns() || row >= rows()) {
		return false;
	}

	auto const availableCells = columns() - column;
	auto cells = static_cast<int>(text.size());
	if (cells > availableCells) {
		cells = availableCells;
	}

	if (fillToEnd || cells == 0) {
		cells = availableCells;
	}

	if (cells <= 0 || cells * cellWidth > maxLinePixels) {
		return false;
	}

	return _drawTextPixels(column * cellWidth, row * cellHeight, cells, text, style);
}

bool Screen::drawMenuItem(int row, std::string_view text, bool selected, Palette const& palette) {
	auto const style = selected
	    ? TextStyle{.foreground = palette.selectedForeground, .background = palette.selectedBackground}
	    : TextStyle{.foreground = palette.foreground, .background = palette.background};
	return drawText(0, row, text, style, true);
}

bool Screen::_drawTextPixels(int x, int y, int cells, std::string_view text, TextStyle style) {
	auto const lineWidth = cells * cellWidth;
	if (x < 0 || y < 0 || lineWidth <= 0 || x + lineWidth > width() || y + cellHeight > height()) {
		return false;
	}

	for (int sourceY = 0; sourceY < cellHeight; ++sourceY) {
		fillLine(_line.data(), lineWidth, style.background);
		for (int cell = 0; cell < cells; ++cell) {
			auto const character = cell < static_cast<int>(text.size()) ? text[static_cast<std::size_t>(cell)] : ' ';
			std::uint8_t glyph[glyphHeight]{};
			_glyphRows(character, glyph);
			auto const glyphRow = sourceY < glyphHeight ? glyph[sourceY] : std::uint8_t{0};
			auto* const cellPixels = _line.data() + cell * cellWidth;
			for (int sourceX = 0; sourceX < glyphWidth; ++sourceX) {
				if ((glyphRow & (1U << (glyphWidth - 1 - sourceX))) != 0) {
					cellPixels[sourceX] = style.foreground;
				}
			}
		}

		if (!_display.blitRgb565({.x = x, .y = y + sourceY, .width = lineWidth, .height = 1}, _line.data(), lineWidth)) {
			return false;
		}
	}

	return true;
}

void Screen::_glyphRows(char character, std::uint8_t (&rows)[glyphHeight]) {
	(void) lexxos::app::picocalc_lcd_console::detail::glyphRows(character, rows);
}

} // namespace lexxos::tui
