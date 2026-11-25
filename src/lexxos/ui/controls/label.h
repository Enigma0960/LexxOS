#pragma once
#include <lexxos/ui/controls/common.h>

namespace lexxos::ui::controls::label {

inline bool render(devices::DisplayDevice& display, Control const& control, ListItems const&, bool) {
	return drawText(display, control.x, control.y, control.text, textColor, canvas, control.width);
}

inline constexpr Handler handler{render, ignoreKey, noNormalization, 18, 9, 16, false, false, true};

} // namespace lexxos::ui::controls::label
