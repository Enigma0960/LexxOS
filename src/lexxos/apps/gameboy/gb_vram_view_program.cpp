#include <lexxos/apps/gameboy/gb_vram_view_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>
#include <lexxos/devices/device_manager.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor GbVramViewProgram::descriptor() {
	return {
	    .name = "gb-vram-view",
	    .help = "show Game Boy VRAM tile data",
	    .description = "Runs a bounded ROM warmup and renders VRAM directly: decoded tile data, BG tile map debug blocks, synthetic map tiles, forced BG output, or a raw VRAM heatmap.",
	    .usage = "gb-vram-view [--mode tiles|map|map-tiles|bg|bg-scan|heat] [--target auto|any|tile-data|map|map0|map1] [--min-target count] [--auto-tile] [--force-visible] [--bg-map auto|9800|9c00] [--bg-tiles auto|8000|8800] [--display crop|fit|none] [--tile number] [--max-cycles count] [--trace] <rom>",
	    .arguments = argumentList(gbVramViewArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GbVramViewProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr) {
		context.writeErrorLine("gb-vram-view: display device is not registered");
		return 1;
	}
	if (!display->isReady()) {
		context.writeErrorLine("gb-vram-view: display device is not ready");
		return 1;
	}
	if (display->nativeFormat() != lexxos::devices::PixelFormat::Rgb565) {
		context.writeErrorLine("gb-vram-view: only rgb565 displays are supported");
		return 1;
	}

	auto displayLock = context.devices().lockDisplayDevice(display->name(), lexxos::devices::DeviceResourcePriority::Interactive, "gb-vram-view");
	if (!displayLock) {
		context.writeErrorLine("gb-vram-view: display device is busy");
		return 1;
	}

	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, arguments.positional("rom"), path)) {
		return 1;
	}

	std::uint32_t firstTile = 0;
	if (!parseUnsigned(arguments.valueOr("tile", "0"), firstTile)) {
		context.writeErrorLine("gb-vram-view: invalid tile number");
		return 2;
	}
	firstTile %= 384U;

	VramViewMode mode = VramViewMode::Tiles;
	if (!parseVramViewMode(arguments.valueOr("mode", "tiles"), mode)) {
		context.writeErrorLine("gb-vram-view: invalid mode, expected tiles, map, map-tiles, bg, bg-scan or heat");
		return 2;
	}

	VramWarmupTarget requestedTarget = VramWarmupTarget::Auto;
	if (!parseVramWarmupTarget(arguments.valueOr("target", "auto"), requestedTarget)) {
		context.writeErrorLine("gb-vram-view: invalid target, expected auto, any, tile-data, map, map0 or map1");
		return 2;
	}
	auto const warmupTarget = effectiveWarmupTarget(mode, requestedTarget);

	std::uint32_t maxWarmupCycles = maxGbEmuWarmupCycles;
	if (!parseUnsigned(arguments.valueOr("max-cycles", "2247168"), maxWarmupCycles)) {
		context.writeErrorLine("gb-vram-view: invalid max-cycles count");
		return 2;
	}
	if (maxWarmupCycles > maxGbEmuWarmupCycles) {
		maxWarmupCycles = maxGbEmuWarmupCycles;
	}

	std::uint32_t minTarget = 1;
	if (!parseUnsigned(arguments.valueOr("min-target", "1"), minTarget)) {
		context.writeErrorLine("gb-vram-view: invalid min-target count");
		return 2;
	}

	lexxos::gameboy::BackgroundMapMode bgMapMode = lexxos::gameboy::BackgroundMapMode::Auto;
	if (!parseBackgroundMapMode(arguments.valueOr("bg-map", "auto"), bgMapMode)) {
		context.writeErrorLine("gb-vram-view: invalid bg-map, expected auto, 9800 or 9c00");
		return 2;
	}
	lexxos::gameboy::BackgroundTileDataMode bgTileMode = lexxos::gameboy::BackgroundTileDataMode::Auto;
	if (!parseBackgroundTileDataMode(arguments.valueOr("bg-tiles", "auto"), bgTileMode)) {
		context.writeErrorLine("gb-vram-view: invalid bg-tiles, expected auto, 8000 or 8800");
		return 2;
	}

	lexxos::gameboy::DisplayBlitMode displayMode = lexxos::gameboy::DisplayBlitMode::Crop;
	if (!parseDisplayBlitMode(arguments.valueOr("display", "crop"), displayMode)) {
		context.writeErrorLine("gb-vram-view: invalid display mode, expected crop, fit or none");
		return 2;
	}

	auto* workspace = gameBoyWorkspace(context);
	if (workspace == nullptr) {
		return 1;
	}

	auto& cartridge = workspace->cartridge;
	if (!openCartridge(context, "gb-vram-view", path.data(), cartridge)) {
		return 1;
	}

	CartridgeLifetimeGuard cartridgeGuard{cartridge};

	if (!cartridge.mapperSupported()) {
		context.writeErrorLine("gb-vram-view: cartridge mapper is not supported yet");
		return 2;
	}

	auto& bus = workspace->bus;
	auto& cpu = workspace->cpu;
	auto& framebuffer = workspace->framebuffer;
	auto& ppu = workspace->ppu;

	bus.reset();
	cartridgeGuard.attachBus(bus);
	cpu.resetToPostBootState();

	auto const warmup = runUntilVramTarget(cpu, bus, maxWarmupCycles, warmupTarget, minTarget);
	if (arguments.has("trace")) {
		context.write("gb-vram-view: warmup steps=");
		writeUnsigned(context, warmup.steps);
		context.write(" cycles=");
		writeUnsigned(context, warmup.cycles);
		context.write(" target=");
		context.write(toString(warmupTarget));
		context.write(" target-nonzero=");
		writeUnsigned(context, countNonZeroForTarget(bus, warmupTarget));
		context.write(" min-target=");
		writeUnsigned(context, minTarget);
		context.write(" status=");
		context.write(lexxos::gameboy::toString(warmup.status));
		context.writeLine("");
		writeRunStopStep(context, "gb-vram-view: ", warmup);
		writeBusIoSnapshot(context, bus, "gb-vram-view: io ");
	}

	auto const firstTileDataByte = firstNonZeroRange(bus, 0x8000, 0x97FF);
	if (mode == VramViewMode::Tiles && arguments.has("auto-tile") && firstTileDataByte.found) {
		firstTile = firstTileDataByte.tile > 4U ? firstTileDataByte.tile - 4U : 0U;
		firstTile %= 384U;
	}

	switch (mode) {
		case VramViewMode::Tiles:
			ppu.renderTileData(bus, framebuffer, static_cast<std::uint16_t>(firstTile));
			break;
		case VramViewMode::Map:
			ppu.renderTileMapDebug(bus, framebuffer);
			break;
		case VramViewMode::MapTiles:
			ppu.renderTileMapPattern(bus, framebuffer);
			break;
		case VramViewMode::Bg: {
			lexxos::gameboy::BackgroundRenderOptions options{
			    .tileDataMode = bgTileMode,
			    .mapMode = bgMapMode,
			    .forceLcdEnable = arguments.has("force-visible"),
			    .forceBgEnable = arguments.has("force-visible"),
			    .forcePalette = arguments.has("force-visible"),
			    .palette = 0xE4,
			};
			ppu.renderBackground(bus, framebuffer, options);
			break;
		}
		case VramViewMode::BgScan:
			ppu.renderBackgroundScan(bus, framebuffer);
			break;
		case VramViewMode::Heat:
			ppu.renderVramHeatmap(bus, framebuffer);
			break;
	}

	lexxos::gameboy::DisplayRenderInfo renderInfo{};
	if (!lexxos::gameboy::Ppu::blitFrameBuffer(*display, framebuffer, &renderInfo, displayMode)) {
		bus.attachCartridge(nullptr);
		context.writeErrorLine("gb-vram-view: failed to render VRAM tile data");
		return 1;
	}

	if (mode == VramViewMode::Tiles) {
		context.write("gb-vram-view: rendered tile data from ");
		context.writeLine(path.data());
	} else {
		context.write("gb-vram-view: rendered ");
		context.write(toString(mode));
		context.write(" view from ");
		context.writeLine(path.data());
	}
	context.write("display: ");
	writeUnsigned(context, static_cast<std::uint32_t>(display->width()));
	context.write("x");
	writeUnsigned(context, static_cast<std::uint32_t>(display->height()));
	context.write(" ");
	context.writeLine(lexxos::devices::toString(display->nativeFormat()));
	context.write("view-mode: ");
	context.write(toString(mode));
	context.writeLine("");
	if (mode == VramViewMode::Tiles) {
		context.write("tile-data: base=0x8000 first-tile=");
		writeUnsigned(context, firstTile);
		context.writeLine(" grid=20x18");
	} else if (mode == VramViewMode::Map) {
		context.write("tile-map: base=");
		context.write((bus.read8OrFF(0xFF40) & 0x08U) != 0 ? "0x9C00" : "0x9800");
		context.writeLine(" debug blocks");
	} else if (mode == VramViewMode::MapTiles) {
		context.write("tile-map-pattern: base=");
		context.write((bus.read8OrFF(0xFF40) & 0x08U) != 0 ? "0x9C00" : "0x9800");
		context.writeLine(" synthetic tile patterns from map indices");
	} else if (mode == VramViewMode::Bg) {
		context.write("background: map=");
		if (bgMapMode == lexxos::gameboy::BackgroundMapMode::Auto) {
			context.write((bus.read8OrFF(0xFF40) & 0x08U) != 0 ? "0x9C00" : "0x9800");
		} else {
			context.write(bgMapMode == lexxos::gameboy::BackgroundMapMode::Map9C00 ? "0x9C00" : "0x9800");
		}
		context.write(" tile-data=");
		if (bgTileMode == lexxos::gameboy::BackgroundTileDataMode::Auto) {
			context.write((bus.read8OrFF(0xFF40) & 0x10U) != 0 ? "0x8000" : "0x8800 signed");
		} else {
			context.write(bgTileMode == lexxos::gameboy::BackgroundTileDataMode::Unsigned8000 ? "0x8000" : "0x8800 signed");
		}
		context.write(" force-visible=");
		context.write(arguments.has("force-visible") ? "1" : "0");
		context.write(" bg-map=");
		context.write(toString(bgMapMode));
		context.write(" bg-tiles=");
		context.writeLine(toString(bgTileMode));
	} else if (mode == VramViewMode::BgScan) {
		context.writeLine("background-scan: TL=9800/8000 TR=9C00/8000 BL=9800/8800 BR=9C00/8800");
	} else {
		context.writeLine("heatmap: VRAM 0x8000-0x9FFF as 128x64 byte map");
	}
	if (firstTileDataByte.found) {
		context.write("tile-data-first-nonzero: address=");
		writeHex16(context, firstTileDataByte.address);
		context.write(" tile=");
		writeUnsigned(context, firstTileDataByte.tile);
		context.write(" value=");
		writeCode8(context, firstTileDataByte.value);
		context.writeLine("");
	}
	context.write("vram-nonzero=");
	writeUnsigned(context, bus.nonZeroVramBytes());
	context.write("/");
	writeUnsigned(context, static_cast<std::uint32_t>(lexxos::gameboy::Bus::vramSize));
	context.write(" vram-writes=");
	writeUnsigned(context, bus.vramWriteCount());
	context.writeLine("");
	writeVramRangeStats(context, bus);
	writeDisplayRenderInfo(context, renderInfo);
	context.writeLine("vram-view: ok");

	if (mode == VramViewMode::Bg) {
		writeBackgroundUsageStats(context, bus, framebuffer, bgMapMode, bgTileMode, "gb-vram-view: ");
	}

	bus.attachCartridge(nullptr);
	return isContinuableEmulationStatus(warmup.status) ? 0 : 2;
}

} // namespace lexxos::apps::gameboy_programs
