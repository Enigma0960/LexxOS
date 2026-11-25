#pragma once

#include <algorithm>
#include <cstdint>

#include <lexxos/ui/controls/registry.h>

namespace lexxos::ui::layout {

// ABI v1 is unchanged. Flag 2 is auto-layout, flag 4 means that an item
// continues the preceding row. Rows and columns are purely kernel geometry.
inline constexpr std::uint8_t autoLayout = 2U;
inline constexpr std::uint8_t sameRow = 4U;
inline constexpr std::uint8_t tabActive = 8U; // Visual state; keyboard focus is separate.
inline constexpr std::uint8_t allowedFlags = 1U | autoLayout | sameRow | tabActive;
inline constexpr int top = 38;
inline constexpr int left = 10;
inline constexpr int gap = 6;

inline bool resolve(Document& document, int width, int height, int& scrollMax, int& listIndex) {
	if (document.version != 1 || document.count > 8 || document.title[31] != '\0' || width < 96 || height < 64 || width > 32767 || height > 32767) {
		return false;
	}

	listIndex = -1;
	for (std::uint16_t i = 0; i < document.count; ++i) {
		auto const& control = document.controls[i];
		auto const* handler = controls::find(control.kind);
		if (control.id == 0 || handler == nullptr || control.text[31] != '\0' || (control.flags & ~allowedFlags) != 0U || ((control.flags & sameRow) != 0U && (control.flags & autoLayout) == 0U)) {
			return false;
		}

		if (control.kind == 7U) {
			if (listIndex >= 0) {
				return false;
			}

			listIndex = i;
		}

		for (std::uint16_t j = 0; j < i; ++j) {
			if (control.id == document.controls[j].id) {
				return false;
			}
		}
	}

	int y = top;
	for (std::uint16_t i = 0; i < document.count;) {
		auto& first = document.controls[i];
		if ((first.flags & autoLayout) == 0U) {
			if ((first.flags & sameRow) != 0U) {
				return false;
			}

			auto const* handler = controls::find(first.kind);
			if (first.x < 0 || first.y < 0 || first.width < handler->minimumWidth || first.height < handler->minimumHeight || first.x > width - first.width || first.y > height - first.height) {
				return false;
			}

			handler->normalize(first, 0U, true);
			++i;
			continue;
		}

		if ((first.flags & sameRow) != 0U) {
			return false; // A row cannot begin with a continuation.
		}

		std::uint16_t end = i + 1;
		while (end < document.count && (document.controls[end].flags & sameRow) != 0U) {
			++end;
		}

		int const columns = end - i;
		int const available = width - 2 * left - gap * (columns - 1);
		if (available <= 0) {
			return false;
		}

		int const columnWidth = available / columns;
		int const extra = available % columns;
		int rowHeight = 0;
		for (std::uint16_t j = i; j < end; ++j) {
			auto& c = document.controls[j];
			auto const* handler = controls::find(c.kind);
			if (c.height <= 0) {
				c.height = handler->defaultHeight;
			}

			int const column = j - i;
			int const w = columnWidth + (column < extra ? 1 : 0);
			if (w < handler->minimumWidth || c.height < handler->minimumHeight || c.height > height - top - 4 || y > 32767 - c.height) {
				return false;
			}

			c.x = static_cast<std::int16_t>(left + column * (columnWidth + gap) + std::min(column, extra));
			c.y = static_cast<std::int16_t>(y);
			c.width = static_cast<std::int16_t>(w);
			rowHeight = std::max(rowHeight, static_cast<int>(c.height));
			handler->normalize(c, 0U, true);
		}

		y += rowHeight + gap;
		i = end;
	}

	scrollMax = std::max(0, y - (height - 4));
	return true;
}

} // namespace lexxos::ui::layout
