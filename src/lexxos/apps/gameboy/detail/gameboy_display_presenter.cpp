#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

std::string_view toString(VramViewMode mode) {
	switch (mode) {
		case VramViewMode::Tiles:
			return "tiles";
		case VramViewMode::Map:
			return "map";
		case VramViewMode::MapTiles:
			return "map-tiles";
		case VramViewMode::Bg:
			return "bg";
		case VramViewMode::BgScan:
			return "bg-scan";
		case VramViewMode::Heat:
			return "heat";
	}
	return "unknown";
}

bool parseVramViewMode(std::string_view value, VramViewMode& mode) {
	if (value == "tiles" || value == "tile") {
		mode = VramViewMode::Tiles;
		return true;
	}
	if (value == "map" || value == "bg-map" || value == "tile-map") {
		mode = VramViewMode::Map;
		return true;
	}
	if (value == "map-tiles" || value == "maptiles" || value == "map-pattern" || value == "pattern-map") {
		mode = VramViewMode::MapTiles;
		return true;
	}
	if (value == "bg" || value == "background" || value == "ppu") {
		mode = VramViewMode::Bg;
		return true;
	}
	if (value == "bg-scan" || value == "bgscan" || value == "scan") {
		mode = VramViewMode::BgScan;
		return true;
	}
	if (value == "heat" || value == "heatmap" || value == "raw") {
		mode = VramViewMode::Heat;
		return true;
	}
	return false;
}

bool parseVramWarmupTarget(std::string_view value, VramWarmupTarget& target) {
	if (value == "auto") {
		target = VramWarmupTarget::Auto;
		return true;
	}
	if (value == "any" || value == "vram") {
		target = VramWarmupTarget::Any;
		return true;
	}
	if (value == "tile-data" || value == "tiles" || value == "tile") {
		target = VramWarmupTarget::TileData;
		return true;
	}
	if (value == "map" || value == "maps") {
		target = VramWarmupTarget::Map;
		return true;
	}
	if (value == "map0" || value == "9800") {
		target = VramWarmupTarget::Map0;
		return true;
	}
	if (value == "map1" || value == "9c00") {
		target = VramWarmupTarget::Map1;
		return true;
	}
	return false;
}

bool parseBackgroundMapMode(std::string_view value, lexxos::gameboy::BackgroundMapMode& mode) {
	if (value == "auto") {
		mode = lexxos::gameboy::BackgroundMapMode::Auto;
		return true;
	}
	if (value == "9800" || value == "map0") {
		mode = lexxos::gameboy::BackgroundMapMode::Map9800;
		return true;
	}
	if (value == "9c00" || value == "9C00" || value == "map1") {
		mode = lexxos::gameboy::BackgroundMapMode::Map9C00;
		return true;
	}
	return false;
}

bool parseBackgroundTileDataMode(std::string_view value, lexxos::gameboy::BackgroundTileDataMode& mode) {
	if (value == "auto") {
		mode = lexxos::gameboy::BackgroundTileDataMode::Auto;
		return true;
	}
	if (value == "8000" || value == "unsigned" || value == "unsigned8000") {
		mode = lexxos::gameboy::BackgroundTileDataMode::Unsigned8000;
		return true;
	}
	if (value == "8800" || value == "signed" || value == "signed8800") {
		mode = lexxos::gameboy::BackgroundTileDataMode::Signed8800;
		return true;
	}
	return false;
}

std::string_view toString(lexxos::gameboy::BackgroundMapMode mode) {
	switch (mode) {
		case lexxos::gameboy::BackgroundMapMode::Auto:
			return "auto";
		case lexxos::gameboy::BackgroundMapMode::Map9800:
			return "9800";
		case lexxos::gameboy::BackgroundMapMode::Map9C00:
			return "9c00";
	}
	return "unknown";
}

std::string_view toString(lexxos::gameboy::BackgroundTileDataMode mode) {
	switch (mode) {
		case lexxos::gameboy::BackgroundTileDataMode::Auto:
			return "auto";
		case lexxos::gameboy::BackgroundTileDataMode::Unsigned8000:
			return "8000";
		case lexxos::gameboy::BackgroundTileDataMode::Signed8800:
			return "8800";
	}
	return "unknown";
}

bool parseDisplayBlitMode(std::string_view value, lexxos::gameboy::DisplayBlitMode& mode) {
	if (value == "crop" || value.empty()) {
		mode = lexxos::gameboy::DisplayBlitMode::Crop;
		return true;
	}
	if (value == "fit") {
		mode = lexxos::gameboy::DisplayBlitMode::Fit;
		return true;
	}
	if (value == "1x" || value == "native" || value == "center") {
		mode = lexxos::gameboy::DisplayBlitMode::Native;
		return true;
	}
	if (value == "none" || value == "off") {
		mode = lexxos::gameboy::DisplayBlitMode::None;
		return true;
	}
	return false;
}

std::string_view toString(lexxos::gameboy::DisplayBlitMode mode) {
	switch (mode) {
		case lexxos::gameboy::DisplayBlitMode::Crop:
			return "crop";
		case lexxos::gameboy::DisplayBlitMode::Fit:
			return "fit";
		case lexxos::gameboy::DisplayBlitMode::Native:
			return "1x";
		case lexxos::gameboy::DisplayBlitMode::None:
			return "none";
	}
	return "unknown";
}

VramWarmupTarget effectiveWarmupTarget(VramViewMode mode, VramWarmupTarget target) {
	if (target != VramWarmupTarget::Auto) {
		return target;
	}
	if (mode == VramViewMode::Tiles) {
		return VramWarmupTarget::TileData;
	}
	if (mode == VramViewMode::Map || mode == VramViewMode::MapTiles) {
		return VramWarmupTarget::Map;
	}
	if (mode == VramViewMode::Bg || mode == VramViewMode::BgScan) {
		return VramWarmupTarget::TileData;
	}
	return VramWarmupTarget::Any;
}

std::string_view toString(VramWarmupTarget target) {
	switch (target) {
		case VramWarmupTarget::Auto:
			return "auto";
		case VramWarmupTarget::Any:
			return "any";
		case VramWarmupTarget::TileData:
			return "tile-data";
		case VramWarmupTarget::Map:
			return "map";
		case VramWarmupTarget::Map0:
			return "map0";
		case VramWarmupTarget::Map1:
			return "map1";
	}
	return "unknown";
}

std::uint16_t skeletonPixel(int x, int y, std::uint32_t frame, lexxos::devices::GamepadState const& gamepad) {
	if ((x / 8 + y / 8 + static_cast<int>(frame)) % 11 == 0) {
		return skeletonShade2;
	}

	if ((x >= 8 && x < 48 && y >= 104 && y < 136) || (x >= 104 && x < 152 && y >= 96 && y < 136)) {
		return skeletonShade1;
	}

	if (gamepad.up && x >= 18 && x < 30 && y >= 104 && y < 116) {
		return skeletonShade3;
	}
	if (gamepad.down && x >= 18 && x < 30 && y >= 124 && y < 136) {
		return skeletonShade3;
	}
	if (gamepad.left && x >= 8 && x < 20 && y >= 114 && y < 126) {
		return skeletonShade3;
	}
	if (gamepad.right && x >= 28 && x < 40 && y >= 114 && y < 126) {
		return skeletonShade3;
	}
	if (gamepad.b && x >= 108 && x < 124 && y >= 114 && y < 130) {
		return skeletonShade3;
	}
	if (gamepad.a && x >= 132 && x < 148 && y >= 106 && y < 122) {
		return skeletonShade3;
	}
	if (gamepad.select && x >= 62 && x < 78 && y >= 124 && y < 130) {
		return skeletonShade3;
	}
	if (gamepad.start && x >= 84 && x < 100 && y >= 124 && y < 130) {
		return skeletonShade3;
	}

	if (((x ^ y ^ static_cast<int>(frame)) & 0x20) != 0) {
		return skeletonShade0;
	}
	return skeletonShade1;
}

bool renderGbSkeletonFrame(lexxos::devices::DisplayDevice& display, std::uint32_t frame, lexxos::devices::GamepadState const& gamepad) {
	if (!display.isReady() || display.nativeFormat() != lexxos::devices::PixelFormat::Rgb565) {
		return false;
	}

	auto const layout = lexxos::gameboy::Ppu::layoutForDisplay(display);
	std::array<std::uint16_t, maxSkeletonLinePixels> line{};

	if (!display.beginFrame()) {
		return false;
	}
	if (!display.clear(skeletonBorderColor)) {
		return false;
	}

	for (int sourceY = 0; sourceY < layout.sourceHeight; ++sourceY) {
		for (int sourceX = 0; sourceX < layout.sourceWidth; ++sourceX) {
			auto const color = skeletonPixel(layout.sourceX + sourceX, layout.sourceY + sourceY, frame, gamepad);
			for (int sx = 0; sx < layout.scale; ++sx) {
				line[static_cast<std::size_t>(sourceX * layout.scale + sx)] = color;
			}
		}

		for (int sy = 0; sy < layout.scale; ++sy) {
			if (!display.blitRgb565(
			        lexxos::devices::Rect{
			            .x = layout.destinationX,
			            .y = layout.destinationY + sourceY * layout.scale + sy,
			            .width = layout.destinationWidth,
			            .height = 1,
			        },
			        line.data(),
			        layout.destinationWidth)) {
				return false;
			}
		}
	}

	// Small dark top stripe inside the visible viewport. It makes the frame visibly distinct from display-test
	// and reserves space for future ROM title/status overlay without adding a font dependency yet.
	line.fill(skeletonDarkColor);
	auto const stripeHeight = 4 * layout.scale;
	for (int y = 0; y < stripeHeight && y < layout.destinationHeight; ++y) {
		if (!display.blitRgb565(
		        lexxos::devices::Rect{.x = layout.destinationX, .y = layout.destinationY + y, .width = layout.destinationWidth, .height = 1},
		        line.data(),
		        layout.destinationWidth)) {
			return false;
		}
	}

	if (!display.endFrame()) {
		return false;
	}

	return true;
}

void writeDisplayRenderInfo(lexxos::os::AppContext& context, lexxos::gameboy::DisplayRenderInfo const& info) {
	context.write("display-mode: ");
	context.write(toString(info.mode));
	context.writeLine("");
	context.write("scale: ");
	writeUnsigned(context, static_cast<std::uint32_t>(info.scale));
	context.writeLine("");
	context.write("source: ");
	writeUnsigned(context, static_cast<std::uint32_t>(info.sourceWidth));
	context.write("x");
	writeUnsigned(context, static_cast<std::uint32_t>(info.sourceHeight));
	context.write(" @ ");
	writeUnsigned(context, static_cast<std::uint32_t>(info.sourceX));
	context.write(",");
	writeUnsigned(context, static_cast<std::uint32_t>(info.sourceY));
	context.writeLine("");
	context.write("destination: ");
	writeUnsigned(context, static_cast<std::uint32_t>(info.destinationWidth));
	context.write("x");
	writeUnsigned(context, static_cast<std::uint32_t>(info.destinationHeight));
	context.write(" @ ");
	writeUnsigned(context, static_cast<std::uint32_t>(info.destinationX));
	context.write(",");
	writeUnsigned(context, static_cast<std::uint32_t>(info.destinationY));
	context.writeLine("");
}

} // namespace lexxos::apps::gameboy_programs
