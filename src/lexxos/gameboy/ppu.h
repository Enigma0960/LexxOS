#pragma once

#include <cstdint>

#include <lexxos/devices/display_device.h>
#include <lexxos/gameboy/bus.h>
#include <lexxos/gameboy/frame_buffer.h>
#include <lexxos/gameboy/ppu_types.h>

namespace lexxos::gameboy {

class Ppu final {
public:
	Ppu() = default;

	void renderBackground(Bus& bus, FrameBuffer& framebuffer) const;
	void renderBackground(Bus& bus, FrameBuffer& framebuffer, BackgroundRenderOptions const& options) const;
	void renderSprites(Bus& bus, FrameBuffer& framebuffer, BackgroundRenderOptions const& options) const;
	void renderFrame(Bus& bus, FrameBuffer& framebuffer, BackgroundRenderOptions const& options) const;
	void renderTileData(Bus& bus, FrameBuffer& framebuffer, std::uint16_t firstTile = 0) const;
	void renderTileMapDebug(Bus& bus, FrameBuffer& framebuffer) const;
	void renderTileMapPattern(Bus& bus, FrameBuffer& framebuffer) const;
	void renderVramHeatmap(Bus& bus, FrameBuffer& framebuffer) const;
	void renderForcedBackground(Bus& bus, FrameBuffer& framebuffer) const;
	void renderBackgroundScan(Bus& bus, FrameBuffer& framebuffer) const;

	[[nodiscard]] static std::uint16_t shadeToRgb565(std::uint8_t shade);
	[[nodiscard]] static std::uint16_t shadeToRgb565(std::uint8_t shade, Rgb565Palette const& palette);
	[[nodiscard]] static DisplayRenderInfo layoutForDisplay(lexxos::devices::DisplayDevice const& display, DisplayBlitMode mode = DisplayBlitMode::Crop);
	[[nodiscard]] static bool blitFrameBuffer(
	    lexxos::devices::DisplayDevice& display,
	    FrameBuffer const& framebuffer,
	    DisplayRenderInfo* info = nullptr,
	    DisplayBlitMode mode = DisplayBlitMode::Crop,
	    bool clearBackground = true,
	    Rgb565Palette const& palette = defaultRgb565Palette);

private:
	[[nodiscard]] static std::uint16_t tileDataAddress(std::uint8_t lcdc, std::uint8_t tileIndex);
	[[nodiscard]] static std::uint16_t tileDataAddress(std::uint8_t lcdc, std::uint8_t tileIndex, BackgroundTileDataMode mode);
	[[nodiscard]] static std::uint16_t tileMapBase(std::uint8_t lcdc, BackgroundMapMode mode);
	[[nodiscard]] static std::uint8_t backgroundPixel(Bus& bus, int x, int y, BackgroundRenderOptions const& options);
	[[nodiscard]] static std::uint8_t mapPalette(std::uint8_t bgp, std::uint8_t colorId);
};

void seedPpuDemoBackground(Bus& bus);

} // namespace lexxos::gameboy
