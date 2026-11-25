#pragma once

#include <lexxos/ui/controls/scroll_list.h>

namespace lexxos::ui::controls::dropdown {

inline constexpr std::uint32_t expanded = 0x80000000U;

inline void normalize(Control& control, std::uint16_t count, bool configuring) {
	if (configuring) {
		return;
	}

	std::uint32_t const flag = control.value & expanded;
	control.value = flag | (count == 0U ? 0U : std::min(control.value & ~expanded, static_cast<std::uint32_t>(count - 1U)));
}

inline bool render(devices::DisplayDevice& display, Control const& control,
    ListItems const& items, bool selected) {
	if (!drawPanel(display, control, selected)) {
		return false;
	}

	std::uint32_t const index = control.value & ~expanded;
	char const* label = index < items.count ? items.entries[index] : "(empty)";
	Control header = control;
	header.height = 28;
	if (!drawText(display, header.x + 5, header.y + 5, label,
	        selected ? canvas : textColor, selected ? accent : panel, header.width - 24)
	    || !drawText(display, header.x + header.width - 18, header.y + 5,
	        (control.value & expanded) != 0U ? "^" : "v",
	        textColor, selected ? accent : panel, 12)) {
		return false;
	}

	if ((control.value & expanded) == 0U) {
		return true;
	}

	int const rows = std::min<int>(items.count, (control.height - 30) / 18);
	for (int row = 0; row < rows; ++row) {
		int const y = control.y + 30 + row * 18;
		bool const active = static_cast<std::uint32_t>(row) == index;
		auto const paper = active ? 0x05FF : panel;
		if (!display.fillRgb565({control.x + 2, y, control.width - 4, 16}, paper) || !drawText(display, control.x + 5, y, items.entries[row], active ? canvas : textColor, paper, control.width - 12)) {
			return false;
		}
	}

	return true;
}

inline bool handle(Control& control, ListItems const& items,
    devices::KeyCode code, char character, Event& event, bool& dirty) {
	if (activate(code, character)) {
		control.value ^= expanded;
		dirty = true;
		event.type = 2U;
		event.value = control.value;
		return true;
	}

	if ((control.value & expanded) == 0U || items.count == 0U) {
		return false;
	}

	Control local = control;
	local.value &= ~expanded;
	bool const result = scroll_list::handle(local, items, code, character, event, dirty);
	if (event.type != 0U && event.value < expanded) {
		control.value = expanded | local.value;
		event.value = control.value;
	}

	return result;
}

inline constexpr Handler handler{render, handle, normalize, 75, 40, 30, true, true, false};

} // namespace lexxos::ui::controls::dropdown
