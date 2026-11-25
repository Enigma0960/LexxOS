#pragma once
#include <lexxos/ui/controls/common.h>

namespace lexxos::ui::controls::slider {

inline bool render(devices::DisplayDevice& display, Control const& control, ListItems const&, bool selected) {
	return renderBar(display, control, selected, true);
}

inline bool key(Control& control, ListItems const&, devices::KeyCode code, char, Event& event, bool& dirty) {
	if (code != devices::KeyCode::Left && code != devices::KeyCode::Right) {
		return false;
	}

	std::uint32_t const previous = control.value;
	if (code == devices::KeyCode::Right) {
		control.value = std::min<std::uint32_t>(100U, control.value + 5U);
	} else {
		control.value = control.value > 5U ? control.value - 5U : 0U;
	}

	if (previous == control.value) {
		return false;
	}

	event.type = 2;
	event.value = control.value;
	dirty = true;
	return true;
}

inline constexpr Handler handler{render, key, clampPercent, 28, 9, 16, true, false, true};

} // namespace lexxos::ui::controls::slider
