#pragma once
#include <lexxos/ui/controls/common.h>

namespace lexxos::ui::controls::list_view {

inline void normalize(Control& control, std::uint16_t itemCount, bool configuring) {
	if (configuring) {
		return;
	}

	control.value = itemCount == 0U ? 0U : std::min(control.value, static_cast<std::uint32_t>(itemCount - 1U));
}

inline bool render(devices::DisplayDevice& display, Control const& control, ListItems const& items, bool selected) {
	if (!drawPanel(display, control, selected)) {
		return false;
	}

	auto const background = selected ? accent : panel;
	if (items.count == 0U) {
		return drawText(display, control.x + 5, control.y + 2, "(empty)", textColor, background, control.width - 10);
	}

	int const rows = 1 + (control.height - 18) / 18;
	int const first = std::max(0, static_cast<int>(control.value) - rows + 1);
	for (int row = 0; row < rows && first + row < items.count; ++row) {
		int const position = first + row;
		int const y = control.y + 2 + row * 18;
		std::uint16_t const paper = position == static_cast<int>(control.value) ? 0x05FF : background;
		if (!display.fillRgb565({control.x + 2, y, control.width - 4, static_cast<int>(font::cellHeight)}, paper)
		    || !drawText(display, control.x + 5, y, items.entries[position],
		        position == static_cast<int>(control.value) ? canvas : textColor, paper, control.width - 10)) {
			return false;
		}
	}

	return true;
}

inline bool key(Control& control, ListItems const& items, devices::KeyCode code, char character, Event& event, bool& dirty) {
	if (items.count == 0U) {
		return false;
	}

	if (code == devices::KeyCode::Up || code == devices::KeyCode::Down) {
		std::uint32_t const previous = control.value;
		if (code == devices::KeyCode::Up && control.value > 0U) {
			--control.value;
		} else if (code == devices::KeyCode::Down && control.value + 1U < items.count) {
			++control.value;
		}

		if (previous == control.value) {
			return false;
		}

		event.type = 2;
		event.value = control.value;
		dirty = true;
		std::memcpy(event.text, items.entries[control.value], sizeof(event.text));
		return true;
	}

	if (!activate(code, character)) {
		return false;
	}

	event.type = 1;
	event.value = control.value;
	std::memcpy(event.text, items.entries[control.value], sizeof(event.text));
	return true;
}

inline constexpr Handler handler{render, key, normalize, 58, 9, 20, true, true, false};

} // namespace lexxos::ui::controls::list_view
