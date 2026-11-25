#pragma once

#include <cstdint>

namespace lexxos::devices {

// Straight alpha8 source-over; channels are blended in RGB565 precision.
// Zero and full opacity are exact. No floating point or dynamic allocation.
[[nodiscard]] inline std::uint16_t blendRgb565(std::uint16_t source, std::uint16_t background, std::uint8_t alpha) {
	if (alpha == 0U) {
		return background;
	}

	if (alpha == 255U) {
		return source;
	}

	auto const blend = [alpha](std::uint32_t s, std::uint32_t b) -> std::uint32_t {
		return (s * alpha + b * (255U - alpha) + 127U) / 255U;
	};
	return static_cast<std::uint16_t>(
	    (blend((source >> 11U) & 31U, (background >> 11U) & 31U) << 11U)
	    | (blend((source >> 5U) & 63U, (background >> 5U) & 63U) << 5U)
	    | blend(source & 31U, background & 31U));
}

} // namespace lexxos::devices
