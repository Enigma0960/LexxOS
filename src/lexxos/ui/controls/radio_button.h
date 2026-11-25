#pragma once

#include <lexxos/ui/controls/common.h>

namespace lexxos::ui::controls::radio_button {

inline void normalize(Control& control, std::uint16_t, bool) {
	control.value = control.value != 0U ? 1U : 0U;
}

inline bool render(devices::DisplayDevice& display, Control const& control,
    ListItems const&, bool selected) {
	if (!drawPanel(display, control, selected)) {
		return false;
	}

	int const x = control.x + 6;
	int const y = control.y + control.height / 2;
	std::uint16_t const background = selected ? accent : panel;
	// A compact pixel-circle: narrower top/bottom rows, solid outline,
	// and a central dot for the selected choice.
	if (!display.fillRgb565({x + 3, y - 5, 5, 1}, textColor) || !display.fillRgb565({x + 1, y - 4, 9, 1}, textColor) || !display.fillRgb565({x, y - 3, 11, 7}, textColor) || !display.fillRgb565({x + 1, y + 4, 9, 1}, textColor) || !display.fillRgb565({x + 3, y + 5, 5, 1}, textColor) || !display.fillRgb565({x + 2, y - 3, 7, 7}, background)) {
		return false;
	}
	if (control.value != 0U && !display.fillRgb565({x + 4, y - 1, 3, 3}, textColor)) {
		return false;
	}
	return drawText(display, x + 17, control.y + (control.height - 16) / 2,
	    control.text, selected ? canvas : textColor, selected ? accent : panel,
	    control.width - 28);
}

inline bool key(Control& control, ListItems const&, devices::KeyCode code,
    char character, Event& event, bool& dirty) {
	if (!activate(code, character)) {
		return false;
	}

	control.value = 1U;
	dirty = true;
	event.type = 2U;
	event.value = 1U;
	return true;
}

inline constexpr Handler handler{render, key, normalize, 28, 32, 20, true, false, true};

} // namespace lexxos::ui::controls::radio_button
