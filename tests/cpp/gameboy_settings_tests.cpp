#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

#include <lexxos/apps/gameboy/detail/gameboy_settings.h>
#include <lexxos/gameboy/bus.h>
#include <lexxos/gameboy/cartridge.h>
#include <lexxos/platform/platform_services.h>

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

class SettingsDisk final : public lexxos::devices::BlockDevice {
public:
	bool initialize() override { return true; }

	bool isReady() const override { return true; }

	std::uint32_t blockSizeBytes() const override { return 512; }

	std::uint64_t blockCount() const override { return 16; }

	bool readBlocks(std::uint64_t, void*, std::size_t) override { return false; }

	bool writeBlocks(std::uint64_t, void const*, std::size_t) override { return false; }
};

class SettingsFilesystem final : public lexxos::vfs::ReadOnlyFilesystem {
public:
	bool isReady() const override { return true; }

	bool isReadOnly() const override { return false; }

	std::string_view typeName() const override { return "settings-test"; }

	bool stat(std::string_view path, lexxos::vfs::Stat& out) override {
		if (path == "/apps" || path == "/apps/gbemu") {
			out = {.type = lexxos::vfs::NodeType::Directory};
			return true;
		}

		if (path != "/apps/gbemu/settings.ini" || content.empty()) {
			return false;
		}

		out = {.type = lexxos::vfs::NodeType::RegularFile, .sizeBytes = content.size()};
		return true;
	}

	bool list(std::string_view, lexxos::vfs::DirectoryVisitor, void*) override { return false; }

	lexxos::vfs::ReadStatus read(std::string_view, std::uint64_t offset, void* buffer,
	    std::size_t capacity, std::size_t& count) override {
		count = offset < content.size() ? std::min(capacity, content.size() - static_cast<std::size_t>(offset)) : 0;
		if (count) {
			std::memcpy(buffer, content.data() + offset, count);
		}
		return lexxos::vfs::ReadStatus::Ok;
	}

	lexxos::vfs::ReadStatus readText(std::string_view, lexxos::vfs::TextWriter, void*) override {
		return lexxos::vfs::ReadStatus::NotFound;
	}

	lexxos::vfs::WriteStatus writeText(std::string_view path, std::string_view text, lexxos::vfs::WriteMode) override {
		if (path != "/apps/gbemu/settings.ini") {
			return lexxos::vfs::WriteStatus::NotFound;
		}
		content.assign(text);
		return lexxos::vfs::WriteStatus::Ok;
	}

	bool sync() override {
		++syncs;
		return syncOk;
	}

	std::string content;
	unsigned syncs{0};
	bool syncOk{true};
};

bool testSettingsVfsIsolation() {
	namespace settings = lexxos::apps::gameboy_programs::detail;
	SettingsDisk disk;
	SettingsFilesystem leftFs, rightFs;
	lexxos::platform::PlatformServices platform{};
	lexxos::vfs::Vfs left, right;
	left.initialize(platform);
	right.initialize(platform);
	if (!expect(left.registerBlockDevice("sd", &disk) && right.registerBlockDevice("sd", &disk), "register independent disks")
	    || !expect(left.mountFilesystem("/dev/sd", "/mnt/sd", &leftFs) == lexxos::vfs::MountStatus::Ok, "mount left settings")
	    || !expect(right.mountFilesystem("/dev/sd", "/mnt/sd", &rightFs) == lexxos::vfs::MountStatus::Ok, "mount right settings")) {
		return false;
	}

	auto created = settings::loadOrCreateGbEmuSettings(left);
	if (!expect(created.status == settings::GbEmuSettingsStatus::CreatedDefault, "create settings in selected VFS")
	    || !expect(rightFs.content.empty() && rightFs.syncs == 0, "default creation does not touch other VFS")) {
		return false;
	}

	auto changed = created.settings;
	changed.renderEvery = 7;
	if (!expect(settings::saveGbEmuSettings(left, changed).status == settings::GbEmuSettingsStatus::Loaded, "save selected VFS")
	    || !expect(settings::loadOrCreateGbEmuSettings(left).settings.renderEvery == 7, "settings round trip")
	    || !expect(settings::loadOrCreateGbEmuSettings(right).settings.renderEvery != 7, "same path is isolated in another VFS")) {
		return false;
	}

	leftFs.syncOk = false;
	return expect(settings::saveGbEmuSettings(left, changed).status == settings::GbEmuSettingsStatus::SyncFailed, "sync failure is preserved")
	    && expect(settings::loadOrCreateGbEmuSettings(right).status == settings::GbEmuSettingsStatus::Loaded, "other VFS remains readable after sync failure");
}

} // namespace

int main() {
	if (!testSettingsVfsIsolation() || !testDefaults() || !testParseOverrides() || !testRejectsInvalidValues() || !testDisplayAliases() || !testCartridgeMapperMetadata() || !testSerialTransferTiming()) {
		return 1;
	}
	return 0;
}
