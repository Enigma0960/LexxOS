#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <lexxos/devices/display_device.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/ui/guest_ui_text.h>
#include <lexxos/ui/guest_ui_theme.h>
#include <lexxos/ui/guest_ui_types.h>

namespace lexxos::ui::controls {

// One stateless descriptor per control type. Widget modules depend only on this
// contract, not on each other or on GuestView. No RTTI, virtual methods or heap.
struct Handler {
	using Render = bool (*)(devices::DisplayDevice&, Control const&, ListItems const&, bool);
	using Key = bool (*)(Control&, ListItems const&, devices::KeyCode, char, Event&, bool&);
	using Normalize = void (*)(Control&, std::uint16_t itemCount, bool configuring);

	Render render;
	Key key;
	Normalize normalize;
	std::int16_t defaultHeight;
	std::int16_t minimumWidth;
	std::int16_t minimumHeight;
	bool focusable;
	bool usesListItems;
	bool verticalNavigation; // Up/Down moves focus instead of editing the control.
};

inline void noNormalization(Control&, std::uint16_t, bool) {
}

inline bool ignoreKey(Control&, ListItems const&, devices::KeyCode, char, Event&, bool&) {
	return false;
}

inline bool activate(devices::KeyCode key, char character) {
	return key == devices::KeyCode::Enter || character == '\r' || character == '\n' || key == devices::KeyCode::Space;
}

inline bool drawPanel(devices::DisplayDevice& display, Control const& control, bool selected) {
	return display.fillRgb565({control.x, control.y, control.width, control.height}, selected ? accent : panel);
}

inline bool drawPanelText(devices::DisplayDevice& display, Control const& control, bool selected, int padding = 5) {
	auto const background = selected ? accent : panel;
	int const maxText = control.width - padding - 2;
	return maxText <= 0 || drawText(display, control.x + padding, control.y + (control.height - static_cast<int>(font::cellHeight)) / 2, control.text, selected ? canvas : textColor, background, maxText);
}

inline void clampPercent(Control& control, std::uint16_t, bool) {
	control.value = std::min<std::uint32_t>(control.value, 100U);
}

inline bool renderBar(devices::DisplayDevice& display, Control const& control, bool selected, bool slider) {
	if (!drawPanel(display, control, selected)) {
		return false;
	}

	int const trackWidth = control.width - 12;
	if (trackWidth <= 0) {
		return true;
	}

	int const filled = (trackWidth * static_cast<int>(std::min<std::uint32_t>(control.value, 100U))) / 100;
	if (!display.fillRgb565({control.x + 6, control.y + control.height / 2 - 2, trackWidth, 4}, canvas)) {
		return false;
	}

	if (filled > 0 && !display.fillRgb565({control.x + 6, control.y + control.height / 2 - 2, filled, 4}, 0x07E0)) {
		return false;
	}

	return !slider || display.fillRgb565({control.x + 6 + filled - 2, control.y + 3, 4, control.height - 6}, textColor);
}

} // namespace lexxos::ui::controls
