#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <lexxos/devices/input_device.h>
#include <lexxos/ui/guest_ui_text.h>
#include <lexxos/ui/guest_ui_theme.h>
#include <lexxos/ui/guest_ui_types.h>

namespace lexxos::ui::dialog {

inline bool validate(DialogDescriptor const& dialog) {
	return dialog.id != 0 && dialog.title[31] == '\0' && dialog.message[95] == '\0' && dialog.kind >= 1U && dialog.kind <= 4U && dialog.buttons >= 1U && dialog.buttons <= 2U;
}

inline std::uint16_t markerColor(std::uint8_t kind) {
	switch (kind) {
		case 1 : return 0x05FF; // Information: blue
		case 2 : return 0xFD20; // Warning: orange
		case 3 : return 0xF800; // Error: red
		case 4 : return 0x07E0; // Question: green
		default: return accent;
	}
}

inline bool render(devices::DisplayDevice& display, DialogDescriptor const& data, std::uint8_t selection) {
	int const width = std::min(display.width() - 12, 296);
	int const height = std::min(display.height() - 12, 164);
	if (width < 94 || height < 108) {
		return false;
	}

	int const x = (display.width() - width) / 2;
	int const y = (display.height() - height) / 2;
	std::uint16_t const color = markerColor(data.kind);
	if (!display.fillRgb565({x - 2, y - 2, width + 4, height + 4}, textColor) || !display.fillRgb565({x, y, width, height}, panel) || !display.fillRgb565({x, y, width, 26}, color) || !drawText(display, x + 6, y + 5, data.title, canvas, color, width - 12)) {
		return false;
	}

	// UTF-8-safe bounded wrapping at glyph boundaries. A separate font manager
	// will eventually provide word wrapping and selectable typefaces.
	char const* p = data.message;
	char const* const end = p + std::strlen(p);
	int const charsPerLine = std::max(1, (width - 20) / static_cast<int>(font::cellWidth));
	int const maxLines = (height - 74) / static_cast<int>(font::cellHeight);
	for (int line = 0; line < maxLines && p < end; ++line) {
		char const* start = p;
		int count = 0;
		while (p < end && count < charsPerLine && *p != '\n') {
			(void) nextCodepoint(p, end);
			++count;
		}

		if (!drawText(display, x + 10, y + 34 + line * static_cast<int>(font::cellHeight),
		        {start, static_cast<std::size_t>(p - start)}, textColor, panel, width - 20)) {
			return false;
		}
		if (p < end && *p == '\n') {
			++p;
		}
	}

	int const buttonY = y + height - 36;
	int const buttonWidth = data.buttons == 2 ? (width - 26) / 2 : width - 20;
	for (int button = 0; button < data.buttons; ++button) {
		int const buttonX = x + 10 + button * (buttonWidth + 6);
		std::uint16_t const bg = button == selection ? accent : titleColor;
		if (!display.fillRgb565({buttonX, buttonY, buttonWidth, 26}, bg) || !drawText(display, buttonX + 7, buttonY + 5, button == 0 ? "OK" : (width < 176 ? "No" : "Cancel"), button == selection ? canvas : textColor, bg, buttonWidth - 12)) {
			return false;
		}
	}

	return true;
}

inline bool key(DialogDescriptor const& dialog, std::uint8_t& selection,
    devices::KeyCode code, char character, Event& event, bool& dirty) {
	if (code == devices::KeyCode::Left || code == devices::KeyCode::Right || code == devices::KeyCode::Tab || character == '\t') {
		if (dialog.buttons == 2U) {
			selection = static_cast<std::uint8_t>(1U - selection);
			dirty = true;
		}

		return true;
	}

	if (code == devices::KeyCode::Escape || character == 0x1B) {
		event.type = 4;
		event.id = dialog.id;
		event.value = dialog.buttons == 2U ? 2U : 1U;
		dirty = true;
		return true;
	}

	if (code == devices::KeyCode::Enter || code == devices::KeyCode::Space || character == '\r' || character == '\n') {
		event.type = 4;
		event.id = dialog.id;
		event.value = selection == 0 ? 1U : 2U;
		dirty = true;
		return true;
	}

	return true; // Modal dialogs consume all other keyboard events.
}

} // namespace lexxos::ui::dialog
