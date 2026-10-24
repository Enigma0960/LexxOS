#include <lexxos/app/detail/picocalc_status_bar_renderer.h>

namespace lexxos::app::picocalc_lcd_console::detail {

void PicoCalcStatusBarRenderer::draw(lexxos::devices::DisplayDevice& display, lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) {
	if (statusBarHeightPixels <= 0 || display.height() <= statusBarHeightPixels) {
		return;
	}

	auto width = display.width();
	if (width > maxStatusBarPixelWidth) {
		width = maxStatusBarPixelWidth;
	}
	if (width <= 0) {
		return;
	}

	auto const rect = lexxos::devices::Rect{
	    .x = 0,
	    .y = 0,
	    .width = width,
	    .height = statusBarHeightPixels,
	};

	if (!display.beginRgb565Stream(rect)) {
		return;
	}

	auto const layout = _layout(status, width);
	std::uint16_t pixels[maxStatusBarPixelWidth]{};
	for (int y = 0; y < statusBarHeightPixels; ++y) {
		for (int x = 0; x < width; ++x) {
			pixels[x] = status.visible && y + 1 == statusBarHeightPixels ? statusSeparatorColor : statusBackgroundColor;
		}
		if (status.visible) {
			_drawComponentsRow(status, layout, y, pixels, width);
		}
		display.writeRgb565Stream(pixels, width);
	}

	display.endRgb565Stream();
}

PicoCalcStatusBarRenderer::Layout PicoCalcStatusBarRenderer::_layout(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status, int width) const {
	Layout layout{};
	_appendTime(layout, status);
	_pushSpring(layout, 1);
	_appendBatteryStatus(layout, status);
	_pushIcon(layout, _batteryIcon);
	_appendWifiStatus(layout, status);

	if (!_resolveSprings(layout, width)) {
		return {};
	}

	return layout;
}

void PicoCalcStatusBarRenderer::_appendTime(Layout& layout, lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const {
	if (!status.timeValid) {
		return;
	}
	char timeText[9]{};
	_formatTimeText(status, timeText);
	_pushText(layout, timeText, statusTextColor);
}

void PicoCalcStatusBarRenderer::_appendBatteryStatus(Layout& layout, lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const {
	if (!status.batteryValid) {
		return;
	}

	char percentText[5]{};
	_formatBatteryPercent(status, percentText);
	_pushText(layout, percentText, _batteryTextColor(status));
	_pushSeparator(layout);
}

void PicoCalcStatusBarRenderer::_appendWifiStatus(Layout& layout, lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const {
	if (!status.wifiValid) {
		return;
	}
	_pushIcon(layout, _wifiIcon);
}

void PicoCalcStatusBarRenderer::_pushText(Layout& layout, char const* text, std::uint16_t color) const {
	if (layout.count >= layout.components.size() || text == nullptr) {
		return;
	}

	auto& component = layout.components[layout.count++];
	component.kind = ComponentKind::Text;
	component.width = _textWidth(text);
	component.color = color;
	std::size_t index = 0;
	for (; index + 1 < sizeof(component.text) && text[index] != '\0'; ++index) {
		component.text[index] = text[index];
	}
	component.text[index] = '\0';
}

void PicoCalcStatusBarRenderer::_pushIcon(Layout& layout, Icon const& icon) const {
	if (layout.count >= layout.components.size()) {
		return;
	}

	auto& component = layout.components[layout.count++];
	component.kind = ComponentKind::Icon;
	component.icon = &icon;
	component.width = icon.width();
}

void PicoCalcStatusBarRenderer::_pushSeparator(Layout& layout) const {
	if (layout.count >= layout.components.size()) {
		return;
	}

	auto& component = layout.components[layout.count++];
	component.kind = ComponentKind::Separator;
	component.width = 1;
	component.color = statusSeparatorColor;
}

void PicoCalcStatusBarRenderer::_pushSpring(Layout& layout, int weight) const {
	if (layout.count >= layout.components.size()) {
		return;
	}

	auto& component = layout.components[layout.count++];
	component.kind = ComponentKind::Spring;
	component.weight = weight;
	component.width = 0;
}

bool PicoCalcStatusBarRenderer::_resolveSprings(Layout& layout, int width) const {
	auto const available = width - statusBarPaddingLeft - statusBarPaddingRight;
	if (available <= 0 || layout.count == 0) {
		layout = Layout{};
		return false;
	}

	int fixedWidth = 0;
	int totalWeight = 0;
	for (std::size_t index = 0; index < layout.count; ++index) {
		fixedWidth += layout.components[index].width;
		if (layout.components[index].kind == ComponentKind::Spring) {
			totalWeight += layout.components[index].weight;
		}
	}

	auto const gapWidth = layout.count > 1 ? static_cast<int>(layout.count - 1) * statusBarComponentGap : 0;
	auto const extra = available - fixedWidth - gapWidth;
	if (extra < 0) {
		layout = Layout{};
		return false;
	}

	auto remainingExtra = extra;
	auto remainingWeight = totalWeight;
	auto cursor = statusBarPaddingLeft;
	for (std::size_t index = 0; index < layout.count; ++index) {
		auto& component = layout.components[index];
		component.x = cursor;
		if (component.kind == ComponentKind::Spring && component.weight > 0 && remainingWeight > 0) {
			auto const share = remainingExtra * component.weight / remainingWeight;
			component.width = share;
			remainingExtra -= share;
			remainingWeight -= component.weight;
		}
		cursor += component.width;
		if (index + 1 < layout.count) {
			cursor += statusBarComponentGap;
		}
	}

	layout.valid = true;
	return true;
}

void PicoCalcStatusBarRenderer::_drawComponentsRow(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status,
    Layout const& layout,
    int rowY,
    std::uint16_t* pixels,
    int width) const {
	if (!layout.valid) {
		return;
	}

	for (std::size_t index = 0; index < layout.count; ++index) {
		auto const& component = layout.components[index];
		if (component.kind == ComponentKind::Icon) {
			if (component.icon != nullptr) {
				component.icon->draw(status, component.x, rowY, pixels, width);
			}
		} else if (component.kind == ComponentKind::Separator) {
			_drawSeparatorRow(component.x, rowY, pixels, width);
		} else if (component.kind == ComponentKind::Text) {
			_drawTextRow(component.text, component.x, statusBarTextY, rowY, component.color, pixels, width);
		}
	}
}

void PicoCalcStatusBarRenderer::_formatBatteryPercent(lexxos::app::picocalc_lcd_console::StatusBarInfo const& status, char (&text)[5]) const {
	if (!status.batteryValid) {
		text[0] = '-';
		text[1] = '-';
		text[2] = '%';
		text[3] = '\0';
		return;
	}

	auto const percent = status.batteryPercent > 100 ? 100 : status.batteryPercent;
	std::size_t index = 0;
	if (percent >= 100) {
		text[index++] = '1';
		text[index++] = '0';
		text[index++] = '0';
	} else if (percent >= 10) {
		text[index++] = static_cast<char>('0' + percent / 10);
		text[index++] = static_cast<char>('0' + percent % 10);
	} else {
		text[index++] = static_cast<char>('0' + percent);
	}
	text[index++] = '%';
	text[index] = '\0';
}

void PicoCalcStatusBarRenderer::_formatTimeText(
    lexxos::app::picocalc_lcd_console::StatusBarInfo const& status,
    char (&text)[9]) const {
	if (!status.timeValid) {
		text[0] = '-';
		text[1] = '-';
		text[2] = ':';
		text[3] = '-';
		text[4] = '-';
		text[5] = ':';
		text[6] = '-';
		text[7] = '-';
		text[8] = '\0';
		return;
	}

	text[0] = static_cast<char>('0' + status.hour / 10);
	text[1] = static_cast<char>('0' + status.hour % 10);
	text[2] = ':';
	text[3] = static_cast<char>('0' + status.minute / 10);
	text[4] = static_cast<char>('0' + status.minute % 10);
	text[5] = ':';
	text[6] = static_cast<char>('0' + status.second / 10);
	text[7] = static_cast<char>('0' + status.second % 10);
	text[8] = '\0';
}

int PicoCalcStatusBarRenderer::_textWidth(char const* text) const {
	if (text == nullptr) {
		return 0;
	}

	int width = 0;
	while (*text != '\0') {
		width += cellWidth;
		++text;
	}
	return width;
}

std::uint16_t PicoCalcStatusBarRenderer::_batteryTextColor(
    lexxos::app::picocalc_lcd_console::StatusBarInfo const& status) const {
	if (!status.batteryValid) {
		return statusSeparatorColor;
	}
	if (status.batteryCharging) {
		return statusAccentColor;
	}
	if (status.batteryPercent <= lowBatteryPercent) {
		return statusLowBatteryColor;
	}
	if (status.batteryPercent <= mediumBatteryPercent) {
		return statusMediumBatteryColor;
	}
	return statusTextColor;
}

void PicoCalcStatusBarRenderer::_drawTextRow(char const* text, int x, int y, int rowY, std::uint16_t color, std::uint16_t* pixels, int width) const {
	if (text == nullptr) {
		return;
	}

	for (int index = 0; text[index] != '\0'; ++index) {
		_drawGlyphRow(text[index], x + index * cellWidth, y, rowY, color, pixels, width);
	}
}

void PicoCalcStatusBarRenderer::_drawGlyphRow(char character, int x, int y, int rowY, std::uint16_t color, std::uint16_t* pixels, int width) const {
	auto const sourceY = rowY - y;
	if (sourceY < 0 || sourceY >= glyphHeight) {
		return;
	}

	std::uint8_t glyph[glyphHeight]{};
	glyphRows(character, glyph);
	for (int sourceX = 0; sourceX < glyphWidth; ++sourceX) {
		bool const active = ((glyph[sourceY] >> (glyphWidth - 1 - sourceX)) & 0x01U) != 0;
		if (active) {
			_setPixel(pixels, width, x + sourceX, color);
		}
	}
}

void PicoCalcStatusBarRenderer::_drawSeparatorRow(int x, int rowY, std::uint16_t* pixels, int width) const {
	constexpr int top = 5;
	constexpr int bottom = statusBarHeightPixels - 6;
	if (rowY >= top && rowY <= bottom) {
		_setPixel(pixels, width, x, statusSeparatorColor);
	}
}

void PicoCalcStatusBarRenderer::_setPixel(std::uint16_t* pixels, int width, int x, std::uint16_t color) const {
	if (pixels != nullptr && x >= 0 && x < width) {
		pixels[x] = color;
	}
}

void PicoCalcStatusBarRenderer::_fillRange(std::uint16_t* pixels, int width, int x0, int x1, std::uint16_t color) const {
	if (pixels == nullptr || width <= 0) {
		return;
	}
	if (x0 < 0) {
		x0 = 0;
	}
	if (x1 >= width) {
		x1 = width - 1;
	}
	for (int x = x0; x <= x1; ++x) {
		pixels[x] = color;
	}
}

} // namespace lexxos::app::picocalc_lcd_console::detail
