#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/display_device.h>
#include <lexxos/ui/noto_sans_mono_9x16.h>

namespace lexxos::ui {

// Bounded UTF-8 decoder for Latin and Cyrillic; unsupported codepoints use '?'.
inline std::uint32_t nextCodepoint(char const*& cursor, char const* end) {
	if (cursor == end) {
		return 0;
	}

	auto const a = static_cast<std::uint8_t>(*cursor++);
	if (a < 0x80) {
		return a;
	}

	if ((a & 0xE0U) == 0xC0U && cursor != end) {
		auto const b = static_cast<std::uint8_t>(*cursor);
		if ((b & 0xC0U) == 0x80U) {
			++cursor;
			std::uint32_t const cp = ((a & 0x1FU) << 6U) | (b & 0x3FU);
			return cp >= 0x80U ? cp : '?';
		}
	}

	if ((a & 0xF0U) == 0xE0U && end - cursor >= 2) {
		auto const b = static_cast<std::uint8_t>(cursor[0]);
		auto const c = static_cast<std::uint8_t>(cursor[1]);
		if ((b & 0xC0U) == 0x80U && (c & 0xC0U) == 0x80U) {
			cursor += 2;
			std::uint32_t const cp = ((a & 0x0FU) << 12U) | ((b & 0x3FU) << 6U) | (c & 0x3FU);
			return cp >= 0x800U && !(cp >= 0xD800U && cp <= 0xDFFFU) ? cp : '?';
		}
	}

	return '?';
}

inline font::Glyph const& glyph(std::uint32_t codepoint) {
	std::size_t left = 0;
	std::size_t right = font::defaultGlyphs.size();
	while (left < right) {
		std::size_t const middle = left + (right - left) / 2;
		if (font::defaultGlyphs[middle].codepoint < codepoint) {
			left = middle + 1;
		} else {
			right = middle;
		}
	}

	if (left < font::defaultGlyphs.size() && font::defaultGlyphs[left].codepoint == codepoint) {
		return font::defaultGlyphs[left];
	}

	return glyph('?');
}

inline bool drawText(devices::DisplayDevice& display, int x, int y, std::string_view text,
    std::uint16_t ink, std::uint16_t paper, int maxWidth) {
	if (x < 0 || y < 0 || x >= display.width() || y + static_cast<int>(font::cellHeight) > display.height()) {
		return false;
	}

	int const maxPixels = std::min({maxWidth, display.width() - x, 320});
	if (maxPixels <= 0) {
		return true;
	}

	std::array<std::uint16_t, 320> line{};
	for (int row = 0; row < static_cast<int>(font::cellHeight); ++row) {
		std::fill_n(line.data(), maxPixels, paper);
		int column = 0;
		auto const* cursor = text.data();
		auto const* end = cursor + text.size();
		while (cursor != end && column < maxPixels) {
			auto const cp = nextCodepoint(cursor, end);
			auto const& entry = glyph(cp);
			for (int cx = 0; cx < static_cast<int>(font::cellWidth) && column + cx < maxPixels; ++cx) {
				if ((entry.bitmap[row] & (1U << cx)) != 0U) {
					line[column + cx] = ink;
				}
			}

			column += static_cast<int>(font::cellWidth);
		}

		if (!display.blitRgb565({x, y + row, maxPixels, 1}, line.data(), maxPixels)) {
			return false;
		}
	}

	return true;
}

} // namespace lexxos::ui
