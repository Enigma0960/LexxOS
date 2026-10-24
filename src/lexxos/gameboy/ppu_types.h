#pragma once

#include <array>
#include <cstdint>

namespace lexxos::gameboy {

inline constexpr int screenWidth = 160;
inline constexpr int screenHeight = 144;
inline constexpr int maxDisplayScale = 2;
inline constexpr int pixelsPerPackedByte = 4;
inline constexpr int frameBufferPackedSize = (screenWidth * screenHeight + pixelsPerPackedByte - 1) / pixelsPerPackedByte;

struct Rgb565Palette {
	std::array<std::uint16_t, 4> shades{0xFFFF, 0xBDF7, 0x7BEF, 0x2104};
};

inline constexpr Rgb565Palette defaultRgb565Palette{};

enum class BackgroundTileDataMode : std::uint8_t {
	Auto,
	Unsigned8000,
	Signed8800,
};

enum class BackgroundMapMode : std::uint8_t {
	Auto,
	Map9800,
	Map9C00,
};

struct BackgroundRenderOptions {
	BackgroundTileDataMode tileDataMode{BackgroundTileDataMode::Auto};
	BackgroundMapMode mapMode{BackgroundMapMode::Auto};
	bool forceLcdEnable{false};
	bool forceBgEnable{false};
	bool forcePalette{false};
	std::uint8_t palette{0xE4};
};

enum class DisplayBlitMode : std::uint8_t {
	Crop,
	Fit,
	Native,
	None,
};

struct DisplayRenderInfo {
	DisplayBlitMode mode{DisplayBlitMode::Crop};
	int scale{1};
	int sourceX{0};
	int sourceY{0};
	int sourceWidth{screenWidth};
	int sourceHeight{screenHeight};
	int destinationX{0};
	int destinationY{0};
	int destinationWidth{screenWidth};
	int destinationHeight{screenHeight};
};

} // namespace lexxos::gameboy

namespace lexxos::gameboy::detail {

struct PackedRgb565Bytes {
	std::array<std::uint8_t, pixelsPerPackedByte * 2> bytes{};
};

} // namespace lexxos::gameboy::detail
