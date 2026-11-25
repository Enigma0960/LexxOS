#pragma once
#include <lexxos/ui/controls/common.h>

namespace lexxos::ui::controls::checkbox {

inline void normalize(Control& control, std::uint16_t, bool configuring) {
	if (configuring) {
		return;
	}
	control.value = control.value != 0U ? 1U : 0U;
}

inline bool render(devices::DisplayDevice& display, Control const& control, ListItems const&, bool selected) {
	return drawPanel(display, control, selected)
	    && display.fillRgb565({control.x + 3, control.y + (control.height - 12) / 2, 12, 12}, control.value ? 0x07E0 : 0x0000)
	    && drawPanelText(display, control, selected, 20);
}

inline bool key(Control& control, ListItems const&, devices::KeyCode code, char character, Event& event, bool& dirty) {
	if (!activate(code, character)) {
		return false;
	}

	control.value = !control.value;
	event.type = 2;
	event.value = control.value;
	dirty = true;
	return true;
}

inline constexpr Handler handler{render, key, normalize, 28, 18, 16, true, false, true};

} // namespace lexxos::ui::controls::checkbox
