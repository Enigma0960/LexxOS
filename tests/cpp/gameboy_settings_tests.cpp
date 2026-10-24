#include <lexxos/apps/gameboy/detail/gameboy_settings.h>
#include <lexxos/gameboy/bus.h>
#include <lexxos/gameboy/cartridge.h>

#include <iostream>
#include <string_view>

namespace {

bool expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "gameboy settings test failed: " << message << '\n';
		return false;
	}
	return true;
}

bool testDefaults() {
	auto settings = lexxos::apps::gameboy_programs::detail::defaultGbEmuSettings();
	return expect(lexxos::apps::gameboy_programs::detail::gbEmuSettingsPath == "/mnt/sd/apps/gbemu/settings.ini", "default path is app gbemu settings")
	    && expect(settings.romDirectoryPath() == "/mnt/sd/gb", "default rom dir is /mnt/sd/gb")
	    && expect(settings.lastRomPath().empty(), "default last rom is empty")
	    && expect(settings.playbackProfileName() == "playable", "default profile is playable")
	    && expect(settings.displayName() == "crop", "default display is crop")
	    && expect(settings.paletteName() == "dmg_green", "default palette is dmg_green")
	    && expect(settings.speedModeName() == "playable", "default speed_mode is playable")
	    && expect(settings.warmupModeName() == "off", "default warmup is off")
	    && expect(settings.romStorageName() == "auto", "default rom_storage is auto")
	    && expect(settings.renderEvery == 4, "default render_every is 4")
	    && expect(settings.inputEvery == 2, "default input_every is 2")
	    && expect(settings.exitHoldMs == 1000, "default exit_hold_ms is 1000")
	    && expect(settings.syncRealtime, "default sync_realtime is true")
	    && expect(!settings.audioEnabled, "default audio is false")
	    && expect(!settings.romFlashReuse, "default rom_flash_reuse is false")
	    && expect(settings.fastTiming, "default fast_timing is true")
	    && expect(settings.fastHalt, "default fast_halt is true")
	    && expect(settings.frames == 16384, "default frames is 16384")
	    && expect(settings.stepsPerFrame == 512, "default steps_per_frame is 512")
	    && expect(settings.cyclesPerFrame == 0, "default cycles_per_frame is 0")
	    && expect(settings.maxWarmupCycles == 8988672, "default max_cycles is 8988672")
	    && expect(settings.minTileData == 256, "default min_tile_data is 256")
	    && expect(!settings.noPpu, "default no_ppu is false")
	    && expect(!settings.debugProfile, "default debug profile is false")
	    && expect(settings.profileEvery == 120, "default profile_every is 120")
	    && expect(!settings.faultTrace, "default fault_trace is false");
}

bool testParseOverrides() {
	lexxos::apps::gameboy_programs::detail::GbEmuSettings settings{};
	constexpr std::string_view text =
	    "[general]\n"
	    "rom_dir = /mnt/sd/roms/gb\n"
	    "last_rom = /mnt/sd/roms/gb/tetris.gbc\n"
	    "profile = none\n"
	    "\n"
	    "[video]\n"
	    "display = fit\n"
	    "render_every = 3\n"
	    "sync_realtime = false\n"
	    "\n"
	    "[palette]\n"
	    "name = high_contrast\n"
	    "\n"
	    "[input]\n"
	    "input_every = 5\n"
	    "exit_combo = select+start\n"
	    "exit_hold_ms = 750\n"
	    "\n"
	    "[audio]\n"
	    "enabled = true\n"
	    "\n"
	    "[performance]\n"
	    "rom_flash_reuse = false\n"
	    "fast_timing = false\n"
	    "fast_halt = false\n"
	    "\n"
	    "[experimental]\n"
	    "speed_mode = max-speed\n"
	    "frames = 4096\n"
	    "steps_per_frame = 2048\n"
	    "cycles_per_frame = 140448\n"
	    "warmup = bg-ready\n"
	    "max_cycles = 17977344\n"
	    "min_tile_data = 512\n"
	    "rom_storage = psram-reuse\n"
	    "no_ppu = true\n"
	    "\n"
	    "[debug]\n"
	    "profile = true\n"
	    "profile_every = 12\n"
	    "fault_trace = true\n";

	return expect(lexxos::apps::gameboy_programs::detail::parseGbEmuSettings(text, settings), "override settings parse")
	    && expect(settings.romDirectoryPath() == "/mnt/sd/roms/gb", "rom_dir override")
	    && expect(settings.lastRomPath() == "/mnt/sd/roms/gb/tetris.gbc", "last_rom override")
	    && expect(settings.playbackProfileName() == "none", "profile override")
	    && expect(settings.displayName() == "fit", "display override")
	    && expect(settings.paletteName() == "high_contrast", "palette override")
	    && expect(settings.speedModeName() == "max-speed", "speed_mode override")
	    && expect(settings.warmupModeName() == "bg-ready", "warmup override")
	    && expect(settings.romStorageName() == "psram-reuse", "rom_storage override")
	    && expect(settings.renderEvery == 3, "render_every override")
	    && expect(settings.inputEvery == 5, "input_every override")
	    && expect(settings.exitHoldMs == 750, "exit_hold_ms override")
	    && expect(!settings.syncRealtime, "sync_realtime override")
	    && expect(settings.audioEnabled, "audio override")
	    && expect(!settings.romFlashReuse, "rom_flash_reuse override")
	    && expect(!settings.fastTiming, "fast_timing override")
	    && expect(!settings.fastHalt, "fast_halt override")
	    && expect(settings.frames == 4096, "frames override")
	    && expect(settings.stepsPerFrame == 2048, "steps_per_frame override")
	    && expect(settings.cyclesPerFrame == 140448, "cycles_per_frame override")
	    && expect(settings.maxWarmupCycles == 17977344, "max_cycles override")
	    && expect(settings.minTileData == 512, "min_tile_data override")
	    && expect(settings.noPpu, "no_ppu override")
	    && expect(settings.debugProfile, "debug profile override")
	    && expect(settings.profileEvery == 12, "profile_every override")
	    && expect(settings.faultTrace, "fault_trace override");
}

bool testRejectsInvalidValues() {
	lexxos::apps::gameboy_programs::detail::GbEmuSettings settings{};
	return expect(
	           !lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("# comment only\n[video]\n", settings),
	           "reject settings without assignments")
	    && expect(
	        !lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[video]\nrender_every = 0\n", settings),
	        "reject zero render_every")
	    && expect(
	        !lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[palette]\nname = unknown\n", settings),
	        "reject unknown palette")
	    && expect(
	        !lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[performance]\nfast_timing = maybe\n", settings),
	        "reject invalid bool")
	    && expect(
	        !lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[audio]\nenabled = maybe\n", settings),
	        "reject invalid audio bool")
	    && expect(
	        !lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[experimental]\nspeed_mode = turbo\n", settings),
	        "reject invalid speed_mode")
	    && expect(
	        lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[experimental]\nframes = 0\n", settings)
	            && settings.frames == 0,
	        "accept zero frames as unlimited")
	    && expect(
	        !lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[experimental]\nsteps_per_frame = 0\n", settings),
	        "reject zero steps_per_frame")
	    && expect(
	        !lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[experimental]\nrom_storage = tape\n", settings),
	        "reject invalid rom_storage");
}

bool testDisplayAliases() {
	lexxos::apps::gameboy_programs::detail::GbEmuSettings settings{};
	return expect(
	           lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[video]\ndisplay = 1x\n", settings)
	               && settings.displayName() == "1x",
	           "accept native 1x display mode")
	    && expect(
	        lexxos::apps::gameboy_programs::detail::parseGbEmuSettings("[video]\ndisplay = native\n", settings)
	            && settings.displayName() == "1x",
	        "canonicalize native display alias");
}

bool testCartridgeMapperMetadata() {
	auto const mbc2Features = lexxos::gameboy::cartridgeFeatures(0x06);
	return expect(lexxos::gameboy::mapperFromCartridgeType(0x05) == lexxos::gameboy::CartridgeMapper::Mbc2, "MBC2 mapper")
	    && expect(lexxos::gameboy::mapperFromCartridgeType(0x06) == lexxos::gameboy::CartridgeMapper::Mbc2, "MBC2 battery mapper")
	    && expect(lexxos::gameboy::mapperName(lexxos::gameboy::CartridgeMapper::Mbc2) == "MBC2", "MBC2 mapper name")
	    && expect(mbc2Features.externalRam, "MBC2 has internal external-RAM address space")
	    && expect(mbc2Features.battery, "MBC2 battery feature");
}

bool testSerialTransferTiming() {
	lexxos::gameboy::Bus bus;
	bus.reset();
	bus.write8Fast(0xFFFF, lexxos::gameboy::interruptSerial);
	bus.write8Fast(0xFF01, 0x42);
	bus.write8Fast(0xFF02, 0x81);
	if (!expect(bus.pendingInterrupts() == 0, "serial internal transfer does not interrupt immediately")
	    || !expect((bus.read8OrFF(0xFF02) & 0x80U) != 0, "serial transfer start bit stays set while active")) {
		return false;
	}
	bus.advanceCycles(4095);
	if (!expect(bus.pendingInterrupts() == 0, "serial internal transfer waits for full byte time")) {
		return false;
	}
	bus.advanceCycles(1);
	if (!expect((bus.pendingInterrupts() & lexxos::gameboy::interruptSerial) != 0, "serial internal transfer requests interrupt on completion")
	    || !expect((bus.read8OrFF(0xFF02) & 0x80U) == 0, "serial transfer start bit clears on completion")
	    || !expect(bus.serialOutputSize() == 1 && bus.serialOutputByte(0) == 0x42, "serial output captures completed byte")) {
		return false;
	}

	bus.reset();
	bus.write8Fast(0xFFFF, lexxos::gameboy::interruptSerial);
	bus.write8Fast(0xFF01, 0x24);
	bus.write8Fast(0xFF02, 0x80);
	bus.advanceCycles(8192);
	return expect(bus.pendingInterrupts() == 0, "serial external clock transfer does not self-complete")
	    && expect((bus.read8OrFF(0xFF02) & 0x80U) != 0, "serial external clock leaves transfer pending")
	    && expect(bus.serialOutputSize() == 0, "serial external clock does not capture without clocks");
}

} // namespace

int main() {
	if (!testDefaults() || !testParseOverrides() || !testRejectsInvalidValues() || !testDisplayAliases() || !testCartridgeMapperMetadata() || !testSerialTransferTiming()) {
		return 1;
	}
	return 0;
}
