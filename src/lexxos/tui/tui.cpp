#include <lexxos/tui/tui.h>

#include <lexxos/app/detail/picocalc_lcd_console_font.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/input_device.h>

namespace lexxos::tui {

// namespace

void MenuState::clamp(std::size_t itemCount, std::size_t visibleRows) {
	if (itemCount == 0) {
		selected = 0;
		scroll = 0;
		return;
	}

	if (selected >= itemCount) {
		selected = itemCount - 1U;
	}

	if (visibleRows == 0) {
		scroll = 0;
		return;
	}

	if (selected < scroll) {
		scroll = selected;
	}

	if (selected >= scroll + visibleRows) {
		scroll = selected - visibleRows + 1U;
	}

	auto const maxScroll = itemCount > visibleRows ? itemCount - visibleRows : std::size_t{0};
	if (scroll > maxScroll) {
		scroll = maxScroll;
	}
}

bool MenuState::moveUp(std::size_t itemCount, std::size_t visibleRows) {
	if (itemCount == 0) {
		return false;
	}

	selected = selected == 0 ? itemCount - 1U : selected - 1U;
	clamp(itemCount, visibleRows);
	return true;
}

bool MenuState::moveDown(std::size_t itemCount, std::size_t visibleRows) {
	if (itemCount == 0) {
		return false;
	}

	selected = (selected + 1U) % itemCount;
	clamp(itemCount, visibleRows);
	return true;
}

} // namespace lexxos::tui
