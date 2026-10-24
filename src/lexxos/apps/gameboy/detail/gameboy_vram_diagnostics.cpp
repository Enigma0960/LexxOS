#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

VramFirstNonZero firstNonZeroRange(lexxos::gameboy::Bus& bus, std::uint16_t start, std::uint16_t endInclusive) {
	for (std::uint32_t address = start; address <= endInclusive; ++address) {
		auto const value = bus.read8OrFF(static_cast<std::uint16_t>(address));
		if (value != 0) {
			return {
			    .found = true,
			    .address = static_cast<std::uint16_t>(address),
			    .tile = static_cast<std::uint16_t>((address - 0x8000U) / 16U),
			    .value = value,
			};
		}
	}
	return {};
}

std::uint32_t countNonZeroRange(lexxos::gameboy::Bus& bus, std::uint16_t start, std::uint16_t endInclusive) {
	std::uint32_t count = 0;
	for (std::uint32_t address = start; address <= endInclusive; ++address) {
		if (bus.read8OrFF(static_cast<std::uint16_t>(address)) != 0) {
			++count;
		}
	}
	return count;
}

std::uint32_t countNonZeroForTarget(lexxos::gameboy::Bus& bus, VramWarmupTarget target) {
	switch (target) {
		case VramWarmupTarget::Auto:
		case VramWarmupTarget::Any:
			return bus.nonZeroVramBytes();
		case VramWarmupTarget::TileData:
			return countNonZeroRange(bus, 0x8000, 0x97FF);
		case VramWarmupTarget::Map:
			return countNonZeroRange(bus, 0x9800, 0x9FFF);
		case VramWarmupTarget::Map0:
			return countNonZeroRange(bus, 0x9800, 0x9BFF);
		case VramWarmupTarget::Map1:
			return countNonZeroRange(bus, 0x9C00, 0x9FFF);
	}
	return bus.nonZeroVramBytes();
}

constexpr std::uint16_t gbBgMap0Address = 0x9800;
constexpr std::uint16_t gbBgMap1Address = 0x9C00;
constexpr std::uint16_t gbTileDataUnsignedAddress = 0x8000;
constexpr std::uint16_t gbTileDataSignedBaseAddress = 0x9000;
constexpr std::uint16_t gbOamAddress = 0xFE00;
constexpr std::uint8_t gbLcdcObjEnable = 0x02;
constexpr std::uint8_t gbLcdcObjSize = 0x04;
constexpr std::uint8_t gbLcdcBgMapSelect = 0x08;
constexpr std::uint8_t gbLcdcTileDataSelect = 0x10;
constexpr std::uint8_t gbLcdcWindowEnable = 0x20;
constexpr std::uint8_t gbLcdcWindowMapSelect = 0x40;
constexpr int gbObjectCount = 40;
constexpr int gbObjectEntrySize = 4;

std::uint16_t backgroundMapBaseFor(std::uint8_t lcdc, lexxos::gameboy::BackgroundMapMode mode) {
	if (mode == lexxos::gameboy::BackgroundMapMode::Map9800) {
		return gbBgMap0Address;
	}
	if (mode == lexxos::gameboy::BackgroundMapMode::Map9C00) {
		return gbBgMap1Address;
	}
	return (lcdc & gbLcdcBgMapSelect) != 0 ? gbBgMap1Address : gbBgMap0Address;
}

bool backgroundTileDataIsSigned(std::uint8_t lcdc, lexxos::gameboy::BackgroundTileDataMode mode) {
	if (mode == lexxos::gameboy::BackgroundTileDataMode::Signed8800) {
		return true;
	}
	if (mode == lexxos::gameboy::BackgroundTileDataMode::Unsigned8000) {
		return false;
	}
	return (lcdc & gbLcdcTileDataSelect) == 0;
}

std::uint16_t backgroundTileDataAddressFor(std::uint8_t lcdc, std::uint8_t tileIndex, lexxos::gameboy::BackgroundTileDataMode mode) {
	if (!backgroundTileDataIsSigned(lcdc, mode)) {
		return static_cast<std::uint16_t>(gbTileDataUnsignedAddress + static_cast<std::uint16_t>(tileIndex) * 16U);
	}
	auto const signedIndex = static_cast<std::int8_t>(tileIndex);
	return static_cast<std::uint16_t>(gbTileDataSignedBaseAddress + static_cast<int>(signedIndex) * 16);
}

std::uint32_t countFrameNonWhitePixels(lexxos::gameboy::FrameBuffer const& framebuffer) {
	std::uint32_t count = 0;
	for (int y = 0; y < lexxos::gameboy::screenHeight; ++y) {
		for (int x = 0; x < lexxos::gameboy::screenWidth; ++x) {
			if (framebuffer.pixel(x, y) != 0) {
				++count;
			}
		}
	}
	return count;
}

BackgroundUsageStats analyzeBackgroundUsage(
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::FrameBuffer const& framebuffer,
    lexxos::gameboy::BackgroundMapMode bgMapMode,
    lexxos::gameboy::BackgroundTileDataMode bgTileMode) {
	BackgroundUsageStats stats{};
	auto const lcdc = bus.read8OrFF(0xFF40);
	stats.bgMapBase = backgroundMapBaseFor(lcdc, bgMapMode);
	stats.windowMapBase = (lcdc & gbLcdcWindowMapSelect) != 0 ? gbBgMap1Address : gbBgMap0Address;
	stats.signedTileData = backgroundTileDataIsSigned(lcdc, bgTileMode);
	stats.frameNonWhitePixels = countFrameNonWhitePixels(framebuffer);

	auto const scy = bus.read8OrFF(0xFF42);
	auto const scx = bus.read8OrFF(0xFF43);
	auto const wy = bus.read8OrFF(0xFF4A);
	auto const wx = bus.read8OrFF(0xFF4B);
	auto const windowLeft = static_cast<int>(wx) - 7;

	for (int tileRow = 0; tileRow < lexxos::gameboy::screenHeight / 8; ++tileRow) {
		for (int tileColumn = 0; tileColumn < lexxos::gameboy::screenWidth / 8; ++tileColumn) {
			auto const screenX = tileColumn * 8;
			auto const screenY = tileRow * 8;
			auto const windowVisible = (lcdc & gbLcdcWindowEnable) != 0 && screenY >= static_cast<int>(wy) && screenX >= windowLeft;
			std::uint16_t mapBase = stats.bgMapBase;
			std::uint8_t sourceX = 0;
			std::uint8_t sourceY = 0;
			if (windowVisible) {
				mapBase = stats.windowMapBase;
				sourceX = static_cast<std::uint8_t>(static_cast<unsigned>(screenX - windowLeft) & 0xFFU);
				sourceY = static_cast<std::uint8_t>(static_cast<unsigned>(screenY - static_cast<int>(wy)) & 0xFFU);
				++stats.windowTileRefs;
			} else {
				sourceX = static_cast<std::uint8_t>(static_cast<unsigned>(screenX + scx) & 0xFFU);
				sourceY = static_cast<std::uint8_t>(static_cast<unsigned>(screenY + scy) & 0xFFU);
			}

			auto const mapX = static_cast<std::uint16_t>(sourceX / 8U);
			auto const mapY = static_cast<std::uint16_t>(sourceY / 8U);
			auto const tileIndex = bus.read8OrFF(static_cast<std::uint16_t>(mapBase + mapY * 32U + mapX));
			auto const tileAddress = backgroundTileDataAddressFor(lcdc, tileIndex, bgTileMode);
			std::uint32_t nonZeroBytes = 0;
			for (std::uint16_t offset = 0; offset < 16U; ++offset) {
				if (bus.read8OrFF(static_cast<std::uint16_t>(tileAddress + offset)) != 0) {
					++nonZeroBytes;
				}
			}
			++stats.tileRefs;
			stats.nonZeroTileBytes += nonZeroBytes;
			if (nonZeroBytes != 0) {
				++stats.nonZeroTileRefs;
			}
		}
	}

	return stats;
}

void writeBackgroundUsageStats(
    lexxos::os::AppContext& context,
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::FrameBuffer const& framebuffer,
    lexxos::gameboy::BackgroundMapMode bgMapMode,
    lexxos::gameboy::BackgroundTileDataMode bgTileMode,
    char const* prefix) {
	auto const stats = analyzeBackgroundUsage(bus, framebuffer, bgMapMode, bgTileMode);
	context.write(prefix);
	context.write("bg-analysis: frame-nonwhite=");
	writeUnsigned(context, stats.frameNonWhitePixels);
	context.write(" map=");
	writeHex16(context, stats.bgMapBase);
	context.write(" window-map=");
	writeHex16(context, stats.windowMapBase);
	context.write(" tile-data=");
	context.write(stats.signedTileData ? "0x8800-signed" : "0x8000");
	context.write(" refs=");
	writeUnsigned(context, stats.tileRefs);
	context.write(" refs-nonzero=");
	writeUnsigned(context, stats.nonZeroTileRefs);
	context.write(" bytes-nonzero=");
	writeUnsigned(context, stats.nonZeroTileBytes);
	context.write(" window-refs=");
	writeUnsigned(context, stats.windowTileRefs);
	context.writeLine("");
}

SpriteUsageStats analyzeSpriteUsage(lexxos::gameboy::Bus& bus) {
	SpriteUsageStats stats{};
	auto const lcdc = bus.read8OrFF(0xFF40);
	stats.enabled = (lcdc & gbLcdcObjEnable) != 0;
	stats.largeSprites = (lcdc & gbLcdcObjSize) != 0;
	auto const spriteHeight = stats.largeSprites ? 16 : 8;

	for (int spriteIndex = 0; spriteIndex < gbObjectCount; ++spriteIndex) {
		auto const entry = static_cast<std::uint16_t>(gbOamAddress + static_cast<std::uint16_t>(spriteIndex * gbObjectEntrySize));
		auto const y = static_cast<int>(bus.read8OrFF(entry)) - 16;
		auto const x = static_cast<int>(bus.read8OrFF(static_cast<std::uint16_t>(entry + 1U))) - 8;
		auto tileIndex = bus.read8OrFF(static_cast<std::uint16_t>(entry + 2U));
		auto const attributes = bus.read8OrFF(static_cast<std::uint16_t>(entry + 3U));

		if (bus.read8OrFF(entry) != 0 || bus.read8OrFF(static_cast<std::uint16_t>(entry + 1U)) != 0 || tileIndex != 0 || attributes != 0) {
			++stats.usedSprites;
		}
		if (x <= -8 || x >= lexxos::gameboy::screenWidth || y <= -spriteHeight || y >= lexxos::gameboy::screenHeight) {
			continue;
		}

		++stats.visibleSprites;
		if (spriteHeight == 16) {
			tileIndex = static_cast<std::uint8_t>(tileIndex & 0xFEU);
		}

		for (int tilePart = 0; tilePart < (spriteHeight == 16 ? 2 : 1); ++tilePart) {
			auto const tile = static_cast<std::uint8_t>(tileIndex + static_cast<std::uint8_t>(tilePart));
			auto const tileAddress = static_cast<std::uint16_t>(gbTileDataUnsignedAddress + static_cast<std::uint16_t>(tile) * 16U);
			std::uint32_t nonZeroBytes = 0;
			for (std::uint16_t offset = 0; offset < 16U; ++offset) {
				if (bus.read8OrFF(static_cast<std::uint16_t>(tileAddress + offset)) != 0) {
					++nonZeroBytes;
				}
			}
			++stats.tileRefs;
			stats.nonZeroTileBytes += nonZeroBytes;
			if (nonZeroBytes != 0) {
				++stats.nonZeroTileRefs;
			}
		}
	}

	return stats;
}

void writeSpriteUsageStats(lexxos::os::AppContext& context, lexxos::gameboy::Bus& bus, char const* prefix) {
	auto const stats = analyzeSpriteUsage(bus);
	context.write(prefix);
	context.write("sprite-analysis: enabled=");
	context.write(stats.enabled ? "1" : "0");
	context.write(" size=");
	context.write(stats.largeSprites ? "8x16" : "8x8");
	context.write(" used=");
	writeUnsigned(context, stats.usedSprites);
	context.write(" visible=");
	writeUnsigned(context, stats.visibleSprites);
	context.write(" refs=");
	writeUnsigned(context, stats.tileRefs);
	context.write(" refs-nonzero=");
	writeUnsigned(context, stats.nonZeroTileRefs);
	context.write(" bytes-nonzero=");
	writeUnsigned(context, stats.nonZeroTileBytes);
	context.writeLine("");
}

void writeVramRangeStats(lexxos::os::AppContext& context, lexxos::gameboy::Bus& bus) {
	auto const tileData = countNonZeroRange(bus, 0x8000, 0x97FF);
	auto const map0 = countNonZeroRange(bus, 0x9800, 0x9BFF);
	auto const map1 = countNonZeroRange(bus, 0x9C00, 0x9FFF);
	context.write("vram-ranges: tile-data=");
	writeUnsigned(context, tileData);
	context.write("/6144 map0=");
	writeUnsigned(context, map0);
	context.write("/1024 map1=");
	writeUnsigned(context, map1);
	context.writeLine("/1024");
}

} // namespace lexxos::apps::gameboy_programs
