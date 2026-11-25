#pragma once
#include <lexxos/ui/controls/common.h>

namespace lexxos::ui::controls::button {

inline bool render(devices::DisplayDevice& display, Control const& control, ListItems const&, bool selected) {
	if ((control.flags & 8U) == 0U) {
		return drawPanel(display, control, selected) && drawPanelText(display, control, selected);
	}

	// Selected tab rises by three pixels. The focus highlight is independent.
	Control raised = control;
	raised.y = static_cast<std::int16_t>(control.y - 3);
	raised.height = static_cast<std::int16_t>(control.height + 3);
	auto const background = selected ? accent : titleColor;
	return display.fillRgb565({raised.x, raised.y, raised.width, raised.height}, background)
	    && drawText(display, raised.x + 5,
	        raised.y + (raised.height - static_cast<int>(font::cellHeight)) / 2,
	        raised.text, selected ? canvas : textColor, background, raised.width - 7);
}

inline bool key(Control&, ListItems const&, devices::KeyCode code, char character, Event& event, bool&) {
	if (!activate(code, character)) {
		return false;
	}

	event.type = 1;
	return true;
}

inline constexpr Handler handler{render, key, noNormalization, 30, 9, 16, true, false, true};

} // namespace lexxos::ui::controls::button
