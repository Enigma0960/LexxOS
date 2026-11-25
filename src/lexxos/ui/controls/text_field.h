#pragma once
#include <lexxos/ui/controls/common.h>

namespace lexxos::ui::controls::text_field {

// Return a UTF-8 codepoint boundary showing the trailing glyphs.
inline char const* visibleSuffix(char const* text, int glyphLimit) {
	char const* const end = text + std::strlen(text);
	int glyphs = 0;
	for (char const* p = text; p < end;) {
		(void) nextCodepoint(p, end);
		++glyphs;
	}

	while (glyphs > glyphLimit && text < end) {
		(void) nextCodepoint(text, end);
		--glyphs;
	}

	return text;
}

inline int caretX(Control const& control) {
	int const available = std::max(0, control.width - 7);
	int const glyphLimit = std::max(0, (available - 2) / static_cast<int>(font::cellWidth));
	char const* text = visibleSuffix(control.text, glyphLimit);
	char const* const end = control.text + std::strlen(control.text);
	int glyphs = 0;
	while (text < end) {
		(void) nextCodepoint(text, end);
		++glyphs;
	}

	return control.x + 5 + glyphs * static_cast<int>(font::cellWidth);
}

inline bool render(devices::DisplayDevice& display, Control const& control, ListItems const&, bool selected) {
	if (!drawPanel(display, control, selected)) {
		return false;
	}

	int const available = control.width - 7;
	if (available <= 0) {
		return true;
	}

	int const cell = static_cast<int>(font::cellWidth);
	int const glyphLimit = selected ? std::max(0, (available - 2) / cell) : available / cell;
	char const* const visible = visibleSuffix(control.text, glyphLimit);
	int const textY = control.y + (control.height - static_cast<int>(font::cellHeight)) / 2;
	auto const background = selected ? accent : panel;
	if (!drawText(display, control.x + 5, textY, visible,
	        selected ? canvas : textColor, background, available)) {
		return false;
	}

	// Steady insertion caret, visible only while this field owns focus.
	return !selected || display.fillRgb565({caretX(control), textY, 2, static_cast<int>(font::cellHeight)}, canvas);
}

inline bool key(Control& control, ListItems const&, devices::KeyCode code, char character, Event& event, bool& dirty) {
	auto length = std::strlen(control.text);
	if ((code == devices::KeyCode::Backspace || character == '' || character == 0x7F) && length > 0) {
		// Delete a complete UTF-8 codepoint instead of leaving a broken suffix.
		do {
			--length;
		} while (length > 0 && (static_cast<std::uint8_t>(control.text[length]) & 0xC0U) == 0x80U);

		control.text[length] = '\0';
		dirty = true;
	} else if (character >= 32 && character <= 126 && length < 31) {
		control.text[length] = character;
		control.text[length + 1] = '\0';
		dirty = true;
	}

	if (!dirty) {
		return false;
	}

	event.type = 2;
	std::memcpy(event.text, control.text, sizeof(event.text));
	return true;
}

inline constexpr Handler handler{render, key, noNormalization, 28, 9, 16, true, false, true};

} // namespace lexxos::ui::controls::text_field
