#pragma once

#include <cstdint>

#include <lexxos/devices/input_device.h>
#include <lexxos/ui/controls/registry.h>
#include <lexxos/ui/guest_ui_types.h>

namespace lexxos::ui::focus {

// Operates on already resolved rectangles, so both v1 and v2 use the same
// navigation policy. No allocations and no dependence on document order.
inline int spatialNeighbor(ResolvedDocument const& document, int focused, devices::KeyCode direction) {
	if (focused < 0 || focused >= static_cast<int>(document.count)) {
		return -1;
	}

	bool const horizontal = direction == devices::KeyCode::Left || direction == devices::KeyCode::Right;
	if (!horizontal && direction != devices::KeyCode::Up && direction != devices::KeyCode::Down) {
		return -1;
	}

	int const sign = (direction == devices::KeyCode::Right || direction == devices::KeyCode::Down) ? 1 : -1;
	Control const& current = document.controls[focused];
	int const centerX = 2 * current.x + current.width;
	int const centerY = 2 * current.y + current.height;
	int best = -1;
	int bestBeam = 2;
	int bestMain = 0x7FFFFFFF;
	int bestCross = 0x7FFFFFFF;

	for (std::uint16_t index = 0; index < document.count; ++index) {
		Control const& candidate = document.controls[index];
		if (index == focused || !controls::canFocus(candidate)) {
			continue;
		}

		int const otherX = 2 * candidate.x + candidate.width;
		int const otherY = 2 * candidate.y + candidate.height;
		int const main = sign * (horizontal ? otherX - centerX : otherY - centerY);
		if (main <= 0) {
			continue;
		}

		bool const overlap = horizontal
		    ? candidate.y < current.y + current.height && current.y < candidate.y + candidate.height
		    : candidate.x < current.x + current.width && current.x < candidate.x + candidate.width;
		// Горизонтальная навигация остаётся в текущем ряду, даже если сосед отключён.
		if (horizontal && !overlap) {
			continue;
		}

		int const beam = overlap ? 0 : 1;
		int const cross = horizontal ? (otherY > centerY ? otherY - centerY : centerY - otherY)
		                             : (otherX > centerX ? otherX - centerX : centerX - otherX);
		if (beam < bestBeam || (beam == bestBeam && (main < bestMain || (main == bestMain && cross < bestCross)))) {
			best = index;
			bestBeam = beam;
			bestMain = main;
			bestCross = cross;
		}
	}

	return best;
}

} // namespace lexxos::ui::focus
