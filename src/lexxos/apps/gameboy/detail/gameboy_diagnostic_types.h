#pragma once

#include <cstdint>

namespace lexxos::apps::gameboy_programs::detail {

struct VramFirstNonZero {
	bool found{false};
	std::uint16_t address{0};
	std::uint16_t tile{0};
	std::uint8_t value{0};
};

struct BackgroundUsageStats {
	std::uint16_t bgMapBase{0x9800};
	std::uint16_t windowMapBase{0x9800};
	bool signedTileData{false};
	std::uint32_t tileRefs{0};
	std::uint32_t nonZeroTileRefs{0};
	std::uint32_t nonZeroTileBytes{0};
	std::uint32_t windowTileRefs{0};
	std::uint32_t frameNonWhitePixels{0};
};

struct SpriteUsageStats {
	bool enabled{false};
	bool largeSprites{false};
	std::uint32_t usedSprites{0};
	std::uint32_t visibleSprites{0};
	std::uint32_t tileRefs{0};
	std::uint32_t nonZeroTileRefs{0};
	std::uint32_t nonZeroTileBytes{0};
};

} // namespace lexxos::apps::gameboy_programs::detail
