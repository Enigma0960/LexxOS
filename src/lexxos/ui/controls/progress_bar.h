#pragma once
#include <lexxos/ui/controls/common.h>

namespace lexxos::ui::controls::progress_bar {

inline bool render(devices::DisplayDevice& display, Control const& control, ListItems const&, bool selected) {
	return renderBar(display, control, selected, false);
}

inline constexpr Handler handler{render, ignoreKey, clampPercent, 20, 9, 16, false, false, true};

} // namespace lexxos::ui::controls::progress_bar
