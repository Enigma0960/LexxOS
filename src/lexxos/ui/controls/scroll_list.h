#pragma once

#include <lexxos/ui/controls/list_view.h>

namespace lexxos::ui::controls::scroll_list {

inline bool key(devices::KeyCode code) {
	return code == devices::KeyCode::Up || code == devices::KeyCode::Down;
}

inline bool handle(Control& control, ListItems const& items,
    devices::KeyCode code, char character, Event& event, bool& dirty) {
	if (items.count != 0U && key(code)) {
		if (code == devices::KeyCode::Up && control.value == 0U) {
			event.type = 2U;
			event.value = 0xFFFFFFFEU; // Request previous virtual row.
			return true;
		}

		if (code == devices::KeyCode::Down && control.value + 1U == items.count) {
			event.type = 2U;
			event.value = 0xFFFFFFFFU; // Request next virtual row.
			return true;
		}
	}

	return list_view::key(control, items, code, character, event, dirty);
}

inline constexpr Handler handler{
    list_view::render, handle, list_view::normalize, 82, 32, 24, true, true, false};

} // namespace lexxos::ui::controls::scroll_list
