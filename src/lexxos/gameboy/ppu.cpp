#include <lexxos/gameboy/ppu.h>
#include <lexxos/gameboy/ppu_types.h>

#include <cstring>

#include <lexxos/kernel/scheduler.h>

namespace lexxos::gameboy {

namespace {

	using detail::PackedRgb565Bytes;

	struct PackedScale2Rgb565Bytes {
		std::array<std::uint8_t, pixelsPerPackedByte * 2 * 2> bytes{};
	};

	constexpr std::uint16_t lcdControlAddress = 0xFF40;
	constexpr std::uint16_t scrollYAddress = 0xFF42;
	constexpr std::uint16_t scrollXAddress = 0xFF43;
	constexpr std::uint16_t bgPaletteAddress = 0xFF47;
	constexpr std::uint16_t windowYAddress = 0xFF4A;
	constexpr std::uint16_t windowXAddress = 0xFF4B;
	constexpr std::uint16_t bgMap0Address = 0x9800;
	constexpr std::uint16_t bgMap1Address = 0x9C00;
	constexpr std::uint16_t tileDataUnsignedAddress = 0x8000;
	constexpr std::uint16_t tileDataSignedBaseAddress = 0x9000;
	constexpr std::uint16_t objectPalette0Address = 0xFF48;
	constexpr std::uint16_t objectPalette1Address = 0xFF49;
	constexpr std::uint16_t oamAddress = 0xFE00;

	constexpr std::uint8_t lcdControlBgEnable = 0x01;
	constexpr std::uint8_t lcdControlObjEnable = 0x02;
	constexpr std::uint8_t lcdControlObjSize = 0x04;
	constexpr std::uint8_t lcdControlBgMapSelect = 0x08;
	constexpr std::uint8_t lcdControlTileDataSelect = 0x10;
	constexpr std::uint8_t lcdControlWindowEnable = 0x20;
	constexpr std::uint8_t lcdControlWindowMapSelect = 0x40;
	constexpr std::uint8_t lcdControlLcdEnable = 0x80;

	constexpr std::uint8_t objectAttributePalette1 = 0x10;
	constexpr std::uint8_t objectAttributeFlipX = 0x20;
	constexpr std::uint8_t objectAttributeFlipY = 0x40;
	constexpr std::uint8_t objectAttributePriority = 0x80;
	constexpr int objectCount = 40;
	constexpr int objectEntrySize = 4;

	constexpr std::uint16_t shadeWhite = 0xFFFF;
	constexpr std::uint16_t shadeLight = 0xBDF7;
	constexpr std::uint16_t shadeDark = 0x7BEF;
	constexpr std::uint16_t shadeBlack = 0x2104;

	constexpr int blitByteRowsPerChunk = 16;

	constexpr std::uint16_t paletteColor(std::uint8_t shade) {
		switch (shade & 0x03U) {
			case 0:
				return shadeWhite;
			case 1:
				return shadeLight;
			case 2:
				return shadeDark;
			case 3:
				return shadeBlack;
		}
		return shadeWhite;
	}

	constexpr void writeRgb565BytePair(PackedRgb565Bytes& entry, int pixel, std::uint16_t color) {
		entry.bytes[static_cast<std::size_t>(pixel * 2)] = static_cast<std::uint8_t>((color >> 8U) & 0xFFU);
		entry.bytes[static_cast<std::size_t>(pixel * 2 + 1)] = static_cast<std::uint8_t>(color & 0xFFU);
	}

	constexpr void writeRgb565Scale2BytePair(PackedScale2Rgb565Bytes& entry, int pixel, std::uint16_t color) {
		auto const offset = static_cast<std::size_t>(pixel * 4);
		entry.bytes[offset] = static_cast<std::uint8_t>((color >> 8U) & 0xFFU);
		entry.bytes[offset + 1U] = static_cast<std::uint8_t>(color & 0xFFU);
		entry.bytes[offset + 2U] = entry.bytes[offset];
		entry.bytes[offset + 3U] = entry.bytes[offset + 1U];
	}

	constexpr PackedRgb565Bytes makePackedRgb565Bytes(std::uint8_t packed) {
		PackedRgb565Bytes entry{};
		writeRgb565BytePair(entry, 0, paletteColor(packed & 0x03U));
		writeRgb565BytePair(entry, 1, paletteColor((packed >> 2U) & 0x03U));
		writeRgb565BytePair(entry, 2, paletteColor((packed >> 4U) & 0x03U));
		writeRgb565BytePair(entry, 3, paletteColor((packed >> 6U) & 0x03U));
		return entry;
	}

	constexpr PackedScale2Rgb565Bytes makePackedScale2Rgb565Bytes(std::uint8_t packed) {
		PackedScale2Rgb565Bytes entry{};
		writeRgb565Scale2BytePair(entry, 0, paletteColor(packed & 0x03U));
		writeRgb565Scale2BytePair(entry, 1, paletteColor((packed >> 2U) & 0x03U));
		writeRgb565Scale2BytePair(entry, 2, paletteColor((packed >> 4U) & 0x03U));
		writeRgb565Scale2BytePair(entry, 3, paletteColor((packed >> 6U) & 0x03U));
		return entry;
	}

	constexpr std::array<PackedRgb565Bytes, 256> makePackedRgb565Lut() {
		std::array<PackedRgb565Bytes, 256> lut{};
		for (std::size_t value = 0; value < lut.size(); ++value) {
			lut[value] = makePackedRgb565Bytes(static_cast<std::uint8_t>(value));
		}
		return lut;
	}

	constexpr std::array<PackedScale2Rgb565Bytes, 256> makePackedScale2Rgb565Lut() {
		std::array<PackedScale2Rgb565Bytes, 256> lut{};
		for (std::size_t value = 0; value < lut.size(); ++value) {
			lut[value] = makePackedScale2Rgb565Bytes(static_cast<std::uint8_t>(value));
		}
		return lut;
	}

	constexpr auto packedRgb565Lut = makePackedRgb565Lut();
	constexpr auto packedScale2Rgb565Lut = makePackedScale2Rgb565Lut();

	bool isDefaultPalette(Rgb565Palette const& palette) {
		return palette.shades == defaultRgb565Palette.shades;
	}

	std::uint16_t dynamicPaletteColor(std::uint8_t shade, Rgb565Palette const& palette) {
		return palette.shades[shade & 0x03U];
	}

	std::uint8_t patternShade(std::uint8_t tile, int x, int y) {
		switch (tile & 0x07U) {
			case 0:
				return static_cast<std::uint8_t>((x + y) & 0x03);
			case 1:
				return static_cast<std::uint8_t>((x / 2) & 0x03);
			case 2:
				return static_cast<std::uint8_t>((y / 2) & 0x03);
			case 3:
				return static_cast<std::uint8_t>(((x ^ y) >> 1) & 0x03);
			case 4:
				return static_cast<std::uint8_t>((x == y || x + y == 7) ? 3 : (tile >> 3U) & 0x03U);
			case 5:
				return static_cast<std::uint8_t>((x == 0 || y == 0 || x == 7 || y == 7) ? 3 : 1);
			case 6:
				return static_cast<std::uint8_t>(((x + y + (tile >> 3U)) / 2) & 0x03);
			case 7:
				return static_cast<std::uint8_t>((x < 4) == (y < 4) ? 0 : 3);
		}
		return 0;
	}

	std::array<std::uint16_t, screenWidth * maxDisplayScale>& blitLineBuffer() {
		static std::array<std::uint16_t, screenWidth * maxDisplayScale> line{};
		return line;
	}

	std::array<std::uint8_t, screenWidth * maxDisplayScale * 2 * blitByteRowsPerChunk>& blitByteRowsBuffer() {
		static std::array<std::uint8_t, screenWidth * maxDisplayScale * 2 * blitByteRowsPerChunk> rows{};
		return rows;
	}

	void unpackPackedLineToRgb565(
	    FrameBuffer const& framebuffer,
	    int sourceX,
	    int sourceY,
	    int width,
	    std::uint16_t* destination,
	    Rgb565Palette const& palette) {
		if (destination == nullptr || width <= 0) {
			return;
		}
		for (int x = 0; x < width; ++x) {
			destination[static_cast<std::size_t>(x)] = dynamicPaletteColor(framebuffer.pixel(sourceX + x, sourceY), palette);
		}
	}

	void writeRgb565Byte(std::uint8_t*& destination, std::uint16_t color) {
		*destination++ = static_cast<std::uint8_t>((color >> 8U) & 0xFFU);
		*destination++ = static_cast<std::uint8_t>(color & 0xFFU);
	}

	void unpackPackedLineToRgb565Bytes(
	    FrameBuffer const& framebuffer,
	    int sourceX,
	    int sourceY,
	    int width,
	    std::uint8_t* destination,
	    Rgb565Palette const& palette) {
		if (destination == nullptr || width <= 0) {
			return;
		}
		auto* out = destination;
		if ((sourceX & 0x03) != 0 || !isDefaultPalette(palette)) {
			for (int x = 0; x < width; ++x) {
				writeRgb565Byte(out, dynamicPaletteColor(framebuffer.pixel(sourceX + x, sourceY), palette));
			}
			return;
		}

		auto const pixelIndex = static_cast<std::size_t>(sourceY * screenWidth + sourceX);
		auto const* source = framebuffer.data() + pixelIndex / pixelsPerPackedByte;
		auto remaining = width;
		while (remaining >= pixelsPerPackedByte) {
			auto const& packedBytes = packedRgb565Lut[*source++].bytes;
			std::memcpy(out, packedBytes.data(), packedBytes.size());
			out += packedBytes.size();
			remaining -= pixelsPerPackedByte;
		}
		for (int x = width - remaining; x < width; ++x) {
			writeRgb565Byte(out, dynamicPaletteColor(framebuffer.pixel(sourceX + x, sourceY), palette));
		}
	}

	void unpackPackedLineToRgb565BytesScale2(
	    FrameBuffer const& framebuffer,
	    int sourceX,
	    int sourceY,
	    int width,
	    std::uint8_t* destination,
	    Rgb565Palette const& palette) {
		if (destination == nullptr || width <= 0) {
			return;
		}

		auto* out = destination;
		if ((sourceX & 0x03) != 0 || !isDefaultPalette(palette)) {
			for (int x = 0; x < width; ++x) {
				auto const color = dynamicPaletteColor(framebuffer.pixel(sourceX + x, sourceY), palette);
				writeRgb565Byte(out, color);
				writeRgb565Byte(out, color);
			}
			return;
		}

		auto const pixelIndex = static_cast<std::size_t>(sourceY * screenWidth + sourceX);
		auto const* source = framebuffer.data() + pixelIndex / pixelsPerPackedByte;
		auto remaining = width;
		while (remaining >= pixelsPerPackedByte) {
			auto const& packedBytes = packedScale2Rgb565Lut[*source++].bytes;
			std::memcpy(out, packedBytes.data(), packedBytes.size());
			out += packedBytes.size();
			remaining -= pixelsPerPackedByte;
		}
		for (int x = width - remaining; x < width; ++x) {
			auto const color = dynamicPaletteColor(framebuffer.pixel(sourceX + x, sourceY), palette);
			writeRgb565Byte(out, color);
			writeRgb565Byte(out, color);
		}
	}

	std::uint8_t tilePixel(Bus& bus, std::uint16_t tileAddress, int column, int row) {
		if (column < 0 || column >= 8 || row < 0 || row >= 8) {
			return 0;
		}

		auto const rowAddress = static_cast<std::uint16_t>(tileAddress + static_cast<std::uint16_t>(row * 2));
		auto const low = bus.read8OrFF(rowAddress);
		auto const high = bus.read8OrFF(static_cast<std::uint16_t>(rowAddress + 1U));
		auto const bit = static_cast<std::uint8_t>(7U - static_cast<std::uint8_t>(column));
		return static_cast<std::uint8_t>(((low >> bit) & 0x01U) | (((high >> bit) & 0x01U) << 1U));
	}
} // namespace

void Ppu::renderBackground(Bus& bus, FrameBuffer& framebuffer) const {
	renderBackground(bus, framebuffer, BackgroundRenderOptions{});
}

void Ppu::renderBackground(Bus& bus, FrameBuffer& framebuffer, BackgroundRenderOptions const& options) const {
	auto lcdc = bus.read8OrFF(lcdControlAddress);
	if (options.forceLcdEnable) {
		lcdc = static_cast<std::uint8_t>(lcdc | lcdControlLcdEnable);
	}
	if (options.forceBgEnable) {
		lcdc = static_cast<std::uint8_t>(lcdc | lcdControlBgEnable);
	}
	if ((lcdc & lcdControlLcdEnable) == 0 || (lcdc & lcdControlBgEnable) == 0) {
		framebuffer.clear(0);
		return;
	}

	auto const bgp = options.forcePalette ? options.palette : bus.read8OrFF(bgPaletteAddress);
	auto const bgMapBase = tileMapBase(lcdc, options.mapMode);
	auto const scy = bus.read8OrFF(scrollYAddress);
	auto const scx = bus.read8OrFF(scrollXAddress);
	auto const windowY = bus.read8OrFF(windowYAddress);
	auto const windowX = bus.read8OrFF(windowXAddress);
	auto const windowLeft = static_cast<int>(windowX) - 7;
	auto const windowEnabled = (lcdc & lcdControlWindowEnable) != 0;
	auto const windowMapBase = (lcdc & lcdControlWindowMapSelect) != 0 ? bgMap1Address : bgMap0Address;

	auto const renderRun = [&](int y, int xBegin, int xEnd, std::uint16_t mapBase, std::uint8_t sourceY, int sourceXBase) {
		int x = xBegin;
		while (x < xEnd) {
			auto const sourceX = static_cast<std::uint8_t>(static_cast<unsigned>(sourceXBase + x) & 0xFFU);
			auto const tileX = static_cast<std::uint16_t>(sourceX >> 3U);
			auto const tileY = static_cast<std::uint16_t>(sourceY >> 3U);
			auto const tileLine = static_cast<std::uint16_t>(sourceY & 0x07U);
			auto const firstColumn = static_cast<int>(sourceX & 0x07U);
			auto pixels = 8 - firstColumn;
			if (x + pixels > xEnd) {
				pixels = xEnd - x;
			}

			auto const tileMapAddress = static_cast<std::uint16_t>(mapBase + tileY * 32U + tileX);
			auto const tileIndex = bus.vramByte(static_cast<std::size_t>(tileMapAddress - Bus::vramStart));
			auto const tileAddress = static_cast<std::uint16_t>(tileDataAddress(lcdc, tileIndex, options.tileDataMode) + tileLine * 2U);
			auto const low = bus.vramByte(static_cast<std::size_t>(tileAddress - Bus::vramStart));
			auto const high = bus.vramByte(static_cast<std::size_t>(tileAddress + 1U - Bus::vramStart));
			for (int column = 0; column < pixels; ++column) {
				auto const bit = static_cast<std::uint8_t>(7U - static_cast<std::uint8_t>(firstColumn + column));
				auto const colorId = static_cast<std::uint8_t>(((low >> bit) & 0x01U) | (((high >> bit) & 0x01U) << 1U));
				framebuffer.setPixel(x + column, y, mapPalette(bgp, colorId));
			}
			x += pixels;
		}
	};

	for (int y = 0; y < screenHeight; ++y) {
		auto const bgSourceY = static_cast<std::uint8_t>(static_cast<unsigned>(y + scy) & 0xFFU);
		if (!windowEnabled || y < static_cast<int>(windowY) || windowLeft >= screenWidth) {
			renderRun(y, 0, screenWidth, bgMapBase, bgSourceY, scx);
			continue;
		}

		auto const splitX = windowLeft <= 0 ? 0 : windowLeft;
		if (splitX > 0) {
			renderRun(y, 0, splitX, bgMapBase, bgSourceY, scx);
		}
		auto const windowSourceY = static_cast<std::uint8_t>(static_cast<unsigned>(y - static_cast<int>(windowY)) & 0xFFU);
		renderRun(y, splitX, screenWidth, windowMapBase, windowSourceY, -windowLeft);
	}
}

void Ppu::renderSprites(Bus& bus, FrameBuffer& framebuffer, BackgroundRenderOptions const& options) const {
	auto lcdc = bus.read8OrFF(lcdControlAddress);
	if (options.forceLcdEnable) {
		lcdc = static_cast<std::uint8_t>(lcdc | lcdControlLcdEnable);
	}
	if ((lcdc & lcdControlLcdEnable) == 0 || (lcdc & lcdControlObjEnable) == 0) {
		return;
	}

	auto const objectHeight = (lcdc & lcdControlObjSize) != 0 ? 16 : 8;
	auto const palette0 = options.forcePalette ? options.palette : bus.read8OrFF(objectPalette0Address);
	auto const palette1 = options.forcePalette ? options.palette : bus.read8OrFF(objectPalette1Address);

	for (int spriteIndex = objectCount - 1; spriteIndex >= 0; --spriteIndex) {
		auto const entry = static_cast<std::size_t>(spriteIndex * objectEntrySize);
		auto const objectY = static_cast<int>(bus.oamByte(entry)) - 16;
		auto const objectX = static_cast<int>(bus.oamByte(entry + 1U)) - 8;
		auto tileIndex = bus.oamByte(entry + 2U);
		auto const attributes = bus.oamByte(entry + 3U);

		if (objectX <= -8 || objectX >= screenWidth || objectY <= -objectHeight || objectY >= screenHeight) {
			continue;
		}

		if (objectHeight == 16) {
			tileIndex = static_cast<std::uint8_t>(tileIndex & 0xFEU);
		}

		auto const palette = (attributes & objectAttributePalette1) != 0 ? palette1 : palette0;
		auto const flipX = (attributes & objectAttributeFlipX) != 0;
		auto const flipY = (attributes & objectAttributeFlipY) != 0;
		auto const behindBackground = (attributes & objectAttributePriority) != 0;

		for (int row = 0; row < objectHeight; ++row) {
			auto const y = objectY + row;
			if (y < 0 || y >= screenHeight) {
				continue;
			}

			auto sourceRow = flipY ? objectHeight - 1 - row : row;
			auto sourceTile = tileIndex;
			if (objectHeight == 16 && sourceRow >= 8) {
				sourceTile = static_cast<std::uint8_t>(sourceTile + 1U);
				sourceRow -= 8;
			}

			auto const tileOffset = static_cast<std::size_t>(sourceTile) * 16U + static_cast<std::size_t>(sourceRow) * 2U;
			auto const low = bus.vramByte(tileOffset);
			auto const high = bus.vramByte(tileOffset + 1U);
			for (int column = 0; column < 8; ++column) {
				auto const x = objectX + column;
				if (x < 0 || x >= screenWidth) {
					continue;
				}

				auto const sourceColumn = flipX ? 7 - column : column;
				auto const bit = static_cast<std::uint8_t>(7U - static_cast<std::uint8_t>(sourceColumn));
				auto const colorId = static_cast<std::uint8_t>(((low >> bit) & 0x01U) | (((high >> bit) & 0x01U) << 1U));
				if (colorId == 0) {
					continue;
				}

				if (behindBackground && framebuffer.pixel(x, y) != 0) {
					continue;
				}

				framebuffer.setPixel(x, y, mapPalette(palette, colorId));
			}
		}
	}
}

void Ppu::renderFrame(Bus& bus, FrameBuffer& framebuffer, BackgroundRenderOptions const& options) const {
	renderBackground(bus, framebuffer, options);
	renderSprites(bus, framebuffer, options);
}

void Ppu::renderTileData(Bus& bus, FrameBuffer& framebuffer, std::uint16_t firstTile) const {
	framebuffer.clear(0);

	constexpr int tilesPerRow = screenWidth / 8;
	constexpr int tileRows = screenHeight / 8;
	constexpr std::uint16_t tileCount = 384;

	for (int tileRow = 0; tileRow < tileRows; ++tileRow) {
		for (int tileColumn = 0; tileColumn < tilesPerRow; ++tileColumn) {
			auto const tileNumber = static_cast<std::uint16_t>((firstTile + static_cast<std::uint16_t>(tileRow * tilesPerRow + tileColumn)) % tileCount);
			auto const tileAddress = static_cast<std::uint16_t>(tileDataUnsignedAddress + tileNumber * 16U);

			for (int row = 0; row < 8; ++row) {
				auto const low = bus.read8OrFF(static_cast<std::uint16_t>(tileAddress + static_cast<std::uint16_t>(row * 2)));
				auto const high = bus.read8OrFF(static_cast<std::uint16_t>(tileAddress + static_cast<std::uint16_t>(row * 2 + 1)));
				for (int column = 0; column < 8; ++column) {
					auto const bit = static_cast<std::uint8_t>(7U - static_cast<std::uint8_t>(column));
					auto const colorId = static_cast<std::uint8_t>(((low >> bit) & 0x01U) | (((high >> bit) & 0x01U) << 1U));
					framebuffer.setPixel(tileColumn * 8 + column, tileRow * 8 + row, colorId);
				}
			}
		}
	}
}

void Ppu::renderTileMapDebug(Bus& bus, FrameBuffer& framebuffer) const {
	framebuffer.clear(0);

	auto const lcdc = bus.read8OrFF(lcdControlAddress);
	auto const mapBase = (lcdc & lcdControlBgMapSelect) != 0 ? bgMap1Address : bgMap0Address;

	for (int tileY = 0; tileY < screenHeight / 8; ++tileY) {
		for (int tileX = 0; tileX < screenWidth / 8; ++tileX) {
			auto const mapX = static_cast<std::uint16_t>(tileX & 31);
			auto const mapY = static_cast<std::uint16_t>(tileY & 31);
			auto const tileValue = bus.read8OrFF(static_cast<std::uint16_t>(mapBase + mapY * 32U + mapX));
			auto const shade = static_cast<std::uint8_t>(tileValue == 0 ? 0 : 1U + (tileValue & 0x03U));

			for (int row = 0; row < 8; ++row) {
				for (int column = 0; column < 8; ++column) {
					auto const border = row == 0 || column == 0;
					framebuffer.setPixel(tileX * 8 + column, tileY * 8 + row, border && tileValue != 0 ? 3 : shade);
				}
			}
		}
	}
}

void Ppu::renderTileMapPattern(Bus& bus, FrameBuffer& framebuffer) const {
	framebuffer.clear(0);

	auto const lcdc = bus.read8OrFF(lcdControlAddress);
	auto const mapBase = (lcdc & lcdControlBgMapSelect) != 0 ? bgMap1Address : bgMap0Address;

	for (int tileY = 0; tileY < screenHeight / 8; ++tileY) {
		for (int tileX = 0; tileX < screenWidth / 8; ++tileX) {
			auto const mapX = static_cast<std::uint16_t>(tileX & 31);
			auto const mapY = static_cast<std::uint16_t>(tileY & 31);
			auto const tileValue = bus.read8OrFF(static_cast<std::uint16_t>(mapBase + mapY * 32U + mapX));

			for (int row = 0; row < 8; ++row) {
				for (int column = 0; column < 8; ++column) {
					std::uint8_t shade = 0;
					if (tileValue != 0) {
						shade = patternShade(tileValue, column, row);
						if (row == 0 || column == 0) {
							shade = 3;
						}
					}
					framebuffer.setPixel(tileX * 8 + column, tileY * 8 + row, shade);
				}
			}
		}
	}
}

void Ppu::renderVramHeatmap(Bus& bus, FrameBuffer& framebuffer) const {
	framebuffer.clear(0);

	// Show the whole 8 KiB VRAM as a 128x64 byte heatmap centered in the 160x144 DMG frame.
	// This intentionally bypasses tile decoding and PPU registers, so even sparse writes are visible.
	constexpr int heatmapWidth = 128;
	constexpr int heatmapHeight = 64;
	constexpr int originX = (screenWidth - heatmapWidth) / 2;
	constexpr int originY = (screenHeight - heatmapHeight) / 2;

	for (int index = 0; index < heatmapWidth * heatmapHeight; ++index) {
		auto const address = static_cast<std::uint16_t>(Bus::vramStart + index);
		auto const value = bus.read8OrFF(address);
		auto const x = originX + index % heatmapWidth;
		auto const y = originY + index / heatmapWidth;
		auto const shade = static_cast<std::uint8_t>(value == 0 ? 0 : 1U + ((value >> 1U) & 0x03U));
		framebuffer.setPixel(x, y, shade);
	}
}

void Ppu::renderForcedBackground(Bus& bus, FrameBuffer& framebuffer) const {
	renderBackground(bus, framebuffer, BackgroundRenderOptions{.forceLcdEnable = true, .forceBgEnable = true, .forcePalette = true, .palette = 0xE4});
}

void Ppu::renderBackgroundScan(Bus& bus, FrameBuffer& framebuffer) const {
	framebuffer.clear(0);

	constexpr int quadrantWidth = screenWidth / 2;
	constexpr int quadrantHeight = screenHeight / 2;
	constexpr BackgroundRenderOptions options[] = {
	    {.tileDataMode = BackgroundTileDataMode::Unsigned8000, .mapMode = BackgroundMapMode::Map9800, .forceLcdEnable = true, .forceBgEnable = true, .forcePalette = true, .palette = 0xE4},
	    {.tileDataMode = BackgroundTileDataMode::Unsigned8000, .mapMode = BackgroundMapMode::Map9C00, .forceLcdEnable = true, .forceBgEnable = true, .forcePalette = true, .palette = 0xE4},
	    {  .tileDataMode = BackgroundTileDataMode::Signed8800, .mapMode = BackgroundMapMode::Map9800, .forceLcdEnable = true, .forceBgEnable = true, .forcePalette = true, .palette = 0xE4},
	    {  .tileDataMode = BackgroundTileDataMode::Signed8800, .mapMode = BackgroundMapMode::Map9C00, .forceLcdEnable = true, .forceBgEnable = true, .forcePalette = true, .palette = 0xE4},
	};

	for (int quadrant = 0; quadrant < 4; ++quadrant) {
		auto const originX = (quadrant & 1) != 0 ? quadrantWidth : 0;
		auto const originY = (quadrant & 2) != 0 ? quadrantHeight : 0;
		for (int y = 0; y < quadrantHeight; ++y) {
			for (int x = 0; x < quadrantWidth; ++x) {
				auto shade = backgroundPixel(bus, x * 2, y * 2, options[quadrant]);
				if (x == 0 || y == 0) {
					shade = 3;
				}
				framebuffer.setPixel(originX + x, originY + y, shade);
			}
		}
	}
}

std::uint16_t Ppu::shadeToRgb565(std::uint8_t shade) {
	return shadeToRgb565(shade, defaultRgb565Palette);
}

std::uint16_t Ppu::shadeToRgb565(std::uint8_t shade, Rgb565Palette const& palette) {
	switch (shade & 0x03U) {
		case 0:
			return palette.shades[0];
		case 1:
			return palette.shades[1];
		case 2:
			return palette.shades[2];
		case 3:
			return palette.shades[3];
	}
	return palette.shades[0];
}

DisplayRenderInfo Ppu::layoutForDisplay(lexxos::devices::DisplayDevice const& display, DisplayBlitMode mode) {
	DisplayRenderInfo info{};
	info.mode = mode;

	if (mode == DisplayBlitMode::None) {
		info.scale = 0;
		info.sourceWidth = 0;
		info.sourceHeight = 0;
		info.destinationWidth = 0;
		info.destinationHeight = 0;
		return info;
	}

	if (mode == DisplayBlitMode::Native && display.width() >= screenWidth && display.height() >= screenHeight) {
		info.scale = 1;
		info.sourceX = 0;
		info.sourceY = 0;
		info.sourceWidth = screenWidth;
		info.sourceHeight = screenHeight;
		info.destinationWidth = screenWidth;
		info.destinationHeight = screenHeight;
		info.destinationX = (display.width() - screenWidth) / 2;
		info.destinationY = (display.height() - screenHeight) / 2;
		return info;
	}

	if (mode == DisplayBlitMode::Fit) {
		auto const displayWidth = display.width();
		auto const displayHeight = display.height();
		if (displayWidth > 0 && displayHeight > 0) {
			auto width = displayWidth;
			auto height = (displayWidth * screenHeight) / screenWidth;
			if (height > displayHeight) {
				height = displayHeight;
				width = (displayHeight * screenWidth) / screenHeight;
			}
			if (width < 1) {
				width = 1;
			}
			if (height < 1) {
				height = 1;
			}
			info.scale = 0;
			info.sourceX = 0;
			info.sourceY = 0;
			info.sourceWidth = screenWidth;
			info.sourceHeight = screenHeight;
			info.destinationWidth = width;
			info.destinationHeight = height;
			info.destinationX = (displayWidth - width) / 2;
			info.destinationY = (displayHeight - height) / 2;
			auto const exactScale = width / screenWidth;
			if (exactScale >= 1
			    && exactScale <= maxDisplayScale
			    && width == screenWidth * exactScale
			    && height == screenHeight * exactScale) {
				info.scale = exactScale;
			}
			return info;
		}
	}

	int scale = display.width() / screenWidth;
	auto const scaleY = display.height() / screenHeight;
	if (scaleY < scale) {
		scale = scaleY;
	}
	if (scale > maxDisplayScale) {
		scale = maxDisplayScale;
	}
	if (scale < 1) {
		scale = 1;
	}
	info.scale = scale;

	info.sourceWidth = display.width() / scale;
	if (info.sourceWidth > screenWidth) {
		info.sourceWidth = screenWidth;
	}
	if (info.sourceWidth < 1) {
		info.sourceWidth = 1;
	}
	info.sourceHeight = display.height() / scale;
	if (info.sourceHeight > screenHeight) {
		info.sourceHeight = screenHeight;
	}
	if (info.sourceHeight < 1) {
		info.sourceHeight = 1;
	}

	info.sourceX = (screenWidth - info.sourceWidth) / 2;
	info.sourceY = (screenHeight - info.sourceHeight) / 2;
	info.destinationWidth = info.sourceWidth * scale;
	info.destinationHeight = info.sourceHeight * scale;
	info.destinationX = (display.width() - info.destinationWidth) / 2;
	info.destinationY = (display.height() - info.destinationHeight) / 2;
	return info;
}

bool Ppu::blitFrameBuffer(
    lexxos::devices::DisplayDevice& display,
    FrameBuffer const& framebuffer,
    DisplayRenderInfo* renderInfo,
    DisplayBlitMode mode,
    bool clearBackground,
    Rgb565Palette const& palette) {
	if (mode == DisplayBlitMode::None) {
		if (renderInfo != nullptr) {
			*renderInfo = layoutForDisplay(display, mode);
		}
		return true;
	}

	if (!display.isReady() || display.nativeFormat() != lexxos::devices::PixelFormat::Rgb565) {
		return false;
	}

	auto const info = layoutForDisplay(display, mode);
	if (renderInfo != nullptr) {
		*renderInfo = info;
	}

	auto& line = blitLineBuffer();
	auto& byteRows = blitByteRowsBuffer();
	if (!display.beginFrame()) {
		return false;
	}

	if (clearBackground && !display.clear(shadeBlack)) {
		return false;
	}

	auto const outputRect = lexxos::devices::Rect{
	    .x = info.destinationX,
	    .y = info.destinationY,
	    .width = info.destinationWidth,
	    .height = info.destinationHeight,
	};
	auto const streaming = display.beginRgb565Stream(outputRect);

	if (info.mode == DisplayBlitMode::Fit && info.scale == 0) {
		for (int destinationY = 0; destinationY < info.destinationHeight; ++destinationY) {
			auto const sourceY = (destinationY * screenHeight) / info.destinationHeight;
			for (int destinationX = 0; destinationX < info.destinationWidth; ++destinationX) {
				auto const sourceX = (destinationX * screenWidth) / info.destinationWidth;
				line[static_cast<std::size_t>(destinationX)] = shadeToRgb565(framebuffer.pixel(sourceX, sourceY), palette);
			}

			if (streaming) {
				if (!display.writeRgb565Stream(line.data(), info.destinationWidth)) {
					display.endRgb565Stream();
					return false;
				}
			} else if (!display.blitRgb565(
			               lexxos::devices::Rect{
			                   .x = info.destinationX,
			                   .y = info.destinationY + destinationY,
			                   .width = info.destinationWidth,
			                   .height = 1,
			               },
			               line.data(),
			               info.destinationWidth)) {
				return false;
			}
			if ((destinationY & 0x07) == 0x07) {
				lexxos::kernel::yield();
			}
		}
	} else if (info.scale == 1 && streaming) {
		for (int sourceY = 0; sourceY < info.sourceHeight;) {
			auto rows = info.sourceHeight - sourceY;
			if (rows > blitByteRowsPerChunk) {
				rows = blitByteRowsPerChunk;
			}

			for (int row = 0; row < rows; ++row) {
				auto* destination = byteRows.data() + static_cast<std::size_t>(row * info.destinationWidth * 2);
				unpackPackedLineToRgb565Bytes(framebuffer, info.sourceX, info.sourceY + sourceY + row, info.sourceWidth, destination, palette);
			}

			if (!display.writeRgb565BytesStream(byteRows.data(), info.destinationWidth * rows)) {
				display.endRgb565Stream();
				return false;
			}

			sourceY += rows;
			lexxos::kernel::yield();
		}
	} else if (info.scale == 2 && streaming) {
		auto const bytesPerDestinationRow = static_cast<std::size_t>(info.destinationWidth * 2);
		auto const sourceRowsPerChunk = blitByteRowsPerChunk / info.scale;
		for (int sourceY = 0; sourceY < info.sourceHeight;) {
			auto rows = info.sourceHeight - sourceY;
			if (rows > sourceRowsPerChunk) {
				rows = sourceRowsPerChunk;
			}

			for (int row = 0; row < rows; ++row) {
				auto* destination = byteRows.data() + static_cast<std::size_t>(row * info.scale) * bytesPerDestinationRow;
				unpackPackedLineToRgb565BytesScale2(framebuffer, info.sourceX, info.sourceY + sourceY + row, info.sourceWidth, destination, palette);
				std::memcpy(destination + bytesPerDestinationRow, destination, bytesPerDestinationRow);
			}

			if (!display.writeRgb565BytesStream(byteRows.data(), info.destinationWidth * rows * info.scale)) {
				display.endRgb565Stream();
				return false;
			}

			sourceY += rows;
			lexxos::kernel::yield();
		}
	} else {
		for (int sourceY = 0; sourceY < info.sourceHeight; ++sourceY) {
			if (info.scale == 1) {
				unpackPackedLineToRgb565(framebuffer, info.sourceX, info.sourceY + sourceY, info.sourceWidth, line.data(), palette);
			} else {
				for (int sourceX = 0; sourceX < info.sourceWidth; ++sourceX) {
					auto const color = shadeToRgb565(framebuffer.pixel(info.sourceX + sourceX, info.sourceY + sourceY), palette);
					for (int sx = 0; sx < info.scale; ++sx) {
						line[static_cast<std::size_t>(sourceX * info.scale + sx)] = color;
					}
				}
			}

			for (int sy = 0; sy < info.scale; ++sy) {
				if (streaming) {
					if (!display.writeRgb565Stream(line.data(), info.destinationWidth)) {
						display.endRgb565Stream();
						return false;
					}
				} else if (!display.blitRgb565(
				               lexxos::devices::Rect{
				                   .x = info.destinationX,
				                   .y = info.destinationY + sourceY * info.scale + sy,
				                   .width = info.destinationWidth,
				                   .height = 1,
				               },
				               line.data(),
				               info.destinationWidth)) {
					return false;
				}
			}
			if ((sourceY & 0x07) == 0x07) {
				lexxos::kernel::yield();
			}
		}
	}

	if (streaming && !display.endRgb565Stream()) {
		return false;
	}

	return display.endFrame();
}

std::uint16_t Ppu::tileDataAddress(std::uint8_t lcdc, std::uint8_t tileIndex) {
	return tileDataAddress(lcdc, tileIndex, BackgroundTileDataMode::Auto);
}

std::uint16_t Ppu::tileDataAddress(std::uint8_t lcdc, std::uint8_t tileIndex, BackgroundTileDataMode mode) {
	if (mode == BackgroundTileDataMode::Unsigned8000 || (mode == BackgroundTileDataMode::Auto && (lcdc & lcdControlTileDataSelect) != 0)) {
		return static_cast<std::uint16_t>(tileDataUnsignedAddress + static_cast<std::uint16_t>(tileIndex) * 16U);
	}

	auto const signedIndex = static_cast<std::int8_t>(tileIndex);
	return static_cast<std::uint16_t>(tileDataSignedBaseAddress + static_cast<int>(signedIndex) * 16);
}

std::uint16_t Ppu::tileMapBase(std::uint8_t lcdc, BackgroundMapMode mode) {
	if (mode == BackgroundMapMode::Map9800) {
		return bgMap0Address;
	}
	if (mode == BackgroundMapMode::Map9C00) {
		return bgMap1Address;
	}
	return (lcdc & lcdControlBgMapSelect) != 0 ? bgMap1Address : bgMap0Address;
}

std::uint8_t Ppu::backgroundPixel(Bus& bus, int x, int y, BackgroundRenderOptions const& options) {
	auto lcdc = bus.read8OrFF(lcdControlAddress);
	if (options.forceLcdEnable) {
		lcdc = static_cast<std::uint8_t>(lcdc | lcdControlLcdEnable);
	}
	if (options.forceBgEnable) {
		lcdc = static_cast<std::uint8_t>(lcdc | lcdControlBgEnable);
	}
	if ((lcdc & lcdControlLcdEnable) == 0 || (lcdc & lcdControlBgEnable) == 0) {
		return 0;
	}

	auto const bgp = options.forcePalette ? options.palette : bus.read8OrFF(bgPaletteAddress);
	auto mapBase = tileMapBase(lcdc, options.mapMode);
	std::uint8_t sourceX = 0;
	std::uint8_t sourceY = 0;
	auto const windowY = bus.read8OrFF(windowYAddress);
	auto const windowX = bus.read8OrFF(windowXAddress);
	auto const windowLeft = static_cast<int>(windowX) - 7;
	auto const windowVisible = (lcdc & lcdControlWindowEnable) != 0 && y >= static_cast<int>(windowY) && x >= windowLeft;
	if (windowVisible) {
		mapBase = (lcdc & lcdControlWindowMapSelect) != 0 ? bgMap1Address : bgMap0Address;
		sourceX = static_cast<std::uint8_t>(static_cast<unsigned>(x - windowLeft) & 0xFFU);
		sourceY = static_cast<std::uint8_t>(static_cast<unsigned>(y - static_cast<int>(windowY)) & 0xFFU);
	} else {
		auto const scy = bus.read8OrFF(scrollYAddress);
		auto const scx = bus.read8OrFF(scrollXAddress);
		sourceY = static_cast<std::uint8_t>(static_cast<unsigned>(y + scy) & 0xFFU);
		sourceX = static_cast<std::uint8_t>(static_cast<unsigned>(x + scx) & 0xFFU);
	}
	auto const tileY = static_cast<std::uint16_t>(sourceY / 8U);
	auto const tileX = static_cast<std::uint16_t>(sourceX / 8U);
	auto const tileLine = static_cast<std::uint16_t>(sourceY % 8U);
	auto const tileMapAddress = static_cast<std::uint16_t>(mapBase + tileY * 32U + tileX);
	auto const tileIndex = bus.read8OrFF(tileMapAddress);
	auto const tileAddress = static_cast<std::uint16_t>(tileDataAddress(lcdc, tileIndex, options.tileDataMode) + tileLine * 2U);
	auto const low = bus.read8OrFF(tileAddress);
	auto const high = bus.read8OrFF(static_cast<std::uint16_t>(tileAddress + 1U));
	auto const bit = static_cast<std::uint8_t>(7U - (sourceX % 8U));
	auto const colorId = static_cast<std::uint8_t>(((low >> bit) & 0x01U) | (((high >> bit) & 0x01U) << 1U));
	return mapPalette(bgp, colorId);
}

std::uint8_t Ppu::mapPalette(std::uint8_t bgp, std::uint8_t colorId) {
	return static_cast<std::uint8_t>((bgp >> ((colorId & 0x03U) * 2U)) & 0x03U);
}

void seedPpuDemoBackground(Bus& bus) {
	bus.reset();
	bus.write8(lcdControlAddress, static_cast<std::uint8_t>(lcdControlLcdEnable | lcdControlTileDataSelect | lcdControlBgEnable));
	bus.write8(scrollXAddress, 0x00);
	bus.write8(scrollYAddress, 0x00);
	bus.write8(bgPaletteAddress, 0xE4);

	for (std::uint16_t tile = 0; tile < 256; ++tile) {
		for (std::uint16_t row = 0; row < 8; ++row) {
			std::uint8_t low = 0;
			std::uint8_t high = 0;
			for (std::uint16_t column = 0; column < 8; ++column) {
				auto const shade = patternShade(static_cast<std::uint8_t>(tile), column, row);
				auto const bit = static_cast<std::uint8_t>(7U - column);
				low = static_cast<std::uint8_t>(low | ((shade & 0x01U) << bit));
				high = static_cast<std::uint8_t>(high | (((shade >> 1U) & 0x01U) << bit));
			}
			auto const address = static_cast<std::uint16_t>(tileDataUnsignedAddress + tile * 16U + row * 2U);
			bus.write8(address, low);
			bus.write8(static_cast<std::uint16_t>(address + 1U), high);
		}
	}

	for (std::uint16_t y = 0; y < 32; ++y) {
		for (std::uint16_t x = 0; x < 32; ++x) {
			auto const tile = static_cast<std::uint8_t>((x + y * 3U) & 0xFFU);
			bus.write8(static_cast<std::uint16_t>(bgMap0Address + y * 32U + x), tile);
		}
	}
}

} // namespace lexxos::gameboy
