#include <lexxos/apps/gameboy/detail/gameboy_settings.h>

#include <cstring>

namespace lexxos::apps::gameboy_programs::detail {

namespace {

	constexpr std::size_t maxSettingsFileBytes = 2048;

	struct SettingsIoWorkspace {
		std::array<char, maxSettingsFileBytes> buffer{};
	};

	SettingsIoWorkspace& settingsIoWorkspace() {
		static SettingsIoWorkspace workspace;
		return workspace;
	}

	enum class SettingsSection : std::uint8_t {
		Unknown,
		General,
		Video,
		Palette,
		Input,
		Audio,
		Performance,
		Experimental,
		Debug,
	};

	template <std::size_t Size>
	bool assignFixed(std::array<char, Size>& output, std::string_view value) {
		if (value.size() >= output.size()) {
			return false;
		}
		std::memset(output.data(), 0, output.size());
		if (!value.empty()) {
			std::memcpy(output.data(), value.data(), value.size());
		}
		output[value.size()] = '\0';
		return true;
	}

	template <std::size_t Size>
	std::string_view fixedView(std::array<char, Size> const& value) {
		return std::string_view{value.data()};
	}

	template <std::size_t Size>
	bool appendFixed(std::array<char, Size>& output, std::size_t& position, std::string_view value) {
		if (position + value.size() >= output.size()) {
			return false;
		}
		if (!value.empty()) {
			std::memcpy(output.data() + position, value.data(), value.size());
			position += value.size();
		}
		output[position] = '\0';
		return true;
	}

	template <std::size_t Size>
	bool appendUnsigned(std::array<char, Size>& output, std::size_t& position, std::uint32_t value) {
		char digits[10]{};
		std::size_t digitCount = 0;
		do {
			digits[digitCount++] = static_cast<char>('0' + (value % 10U));
			value /= 10U;
		} while (value != 0 && digitCount < sizeof(digits));

		for (std::size_t index = 0; index < digitCount; ++index) {
			if (!appendFixed(output, position, std::string_view{&digits[digitCount - index - 1U], 1})) {
				return false;
			}
		}
		return true;
	}

	template <std::size_t Size>
	bool appendBool(std::array<char, Size>& output, std::size_t& position, bool value) {
		return appendFixed(output, position, value ? "true" : "false");
	}

	bool appendSettingsContent(GbEmuSettings const& settings, std::array<char, maxSettingsFileBytes>& output, std::size_t& position) {
		return appendFixed(output, position, "[general]\n")
		    && appendFixed(output, position, "rom_dir = ")
		    && appendFixed(output, position, settings.romDirectoryPath())
		    && appendFixed(output, position, "\nlast_rom = ")
		    && appendFixed(output, position, settings.lastRomPath())
		    && appendFixed(output, position, "\nprofile = ")
		    && appendFixed(output, position, settings.playbackProfileName())
		    && appendFixed(output, position, "\n\n[video]\ndisplay = ")
		    && appendFixed(output, position, settings.displayName())
		    && appendFixed(output, position, "\nrender_every = ")
		    && appendUnsigned(output, position, settings.renderEvery)
		    && appendFixed(output, position, "\nsync_realtime = ")
		    && appendBool(output, position, settings.syncRealtime)
		    && appendFixed(output, position, "\n\n[palette]\nname = ")
		    && appendFixed(output, position, settings.paletteName())
		    && appendFixed(output, position, "\n\n[input]\ninput_every = ")
		    && appendUnsigned(output, position, settings.inputEvery)
		    && appendFixed(output, position, "\nexit_combo = start+select\nexit_hold_ms = ")
		    && appendUnsigned(output, position, settings.exitHoldMs)
		    && appendFixed(output, position, "\n\n[audio]\nenabled = ")
		    && appendBool(output, position, settings.audioEnabled)
		    && appendFixed(output, position, "\n\n[performance]\nrom_flash_reuse = ")
		    && appendBool(output, position, settings.romFlashReuse)
		    && appendFixed(output, position, "\nfast_timing = ")
		    && appendBool(output, position, settings.fastTiming)
		    && appendFixed(output, position, "\nfast_halt = ")
		    && appendBool(output, position, settings.fastHalt)
		    && appendFixed(output, position, "\n\n[experimental]\nspeed_mode = ")
		    && appendFixed(output, position, settings.speedModeName())
		    && appendFixed(output, position, "\nframes = ")
		    && appendUnsigned(output, position, settings.frames)
		    && appendFixed(output, position, "\nsteps_per_frame = ")
		    && appendUnsigned(output, position, settings.stepsPerFrame)
		    && appendFixed(output, position, "\ncycles_per_frame = ")
		    && appendUnsigned(output, position, settings.cyclesPerFrame)
		    && appendFixed(output, position, "\nwarmup = ")
		    && appendFixed(output, position, settings.warmupModeName())
		    && appendFixed(output, position, "\nmax_cycles = ")
		    && appendUnsigned(output, position, settings.maxWarmupCycles)
		    && appendFixed(output, position, "\nmin_tile_data = ")
		    && appendUnsigned(output, position, settings.minTileData)
		    && appendFixed(output, position, "\nrom_storage = ")
		    && appendFixed(output, position, settings.romStorageName())
		    && appendFixed(output, position, "\nno_ppu = ")
		    && appendBool(output, position, settings.noPpu)
		    && appendFixed(output, position, "\n\n[debug]\nprofile = ")
		    && appendBool(output, position, settings.debugProfile)
		    && appendFixed(output, position, "\nprofile_every = ")
		    && appendUnsigned(output, position, settings.profileEvery)
		    && appendFixed(output, position, "\nfault_trace = ")
		    && appendBool(output, position, settings.faultTrace)
		    && appendFixed(output, position, "\n");
	}

	std::string_view trim(std::string_view value) {
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
			value.remove_prefix(1);
		}
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
			value.remove_suffix(1);
		}
		return value;
	}

	bool parseUnsigned(std::string_view value, std::uint32_t& output) {
		if (value.empty()) {
			return false;
		}
		std::uint32_t result = 0;
		for (char const character : value) {
			if (character < '0' || character > '9') {
				return false;
			}
			auto const digit = static_cast<std::uint32_t>(character - '0');
			if (result > (0xFFFFFFFFU - digit) / 10U) {
				return false;
			}
			result = result * 10U + digit;
		}
		output = result;
		return true;
	}

	bool parseBool(std::string_view value, bool& output) {
		if (value == "true" || value == "on" || value == "yes" || value == "1") {
			output = true;
			return true;
		}
		if (value == "false" || value == "off" || value == "no" || value == "0") {
			output = false;
			return true;
		}
		return false;
	}

	bool isKnownDisplay(std::string_view value) {
		return value == "crop" || value == "fit" || value == "1x" || value == "native" || value == "center" || value == "none" || value == "off";
	}

	std::string_view canonicalDisplay(std::string_view value) {
		if (value == "native" || value == "center") {
			return "1x";
		}
		if (value == "off") {
			return "none";
		}
		return value;
	}

	bool isKnownPalette(std::string_view value) {
		return value == "default" || value == "gray" || value == "grayscale" || value == "dmg_green" || value == "pocket_gray" || value == "gbc_blue" || value == "gbc_brown" || value == "gbc_red" || value == "gbc_pastel" || value == "high_contrast";
	}

	bool isKnownSpeedMode(std::string_view value) {
		return value == "playable" || value == "playable-fast" || value == "max-speed" || value == "psram-fast" || value == "manual";
	}

	bool isKnownWarmupMode(std::string_view value) {
		return value == "off" || value == "vram" || value == "visible" || value == "tile-data" || value == "bg-ready";
	}

	bool isKnownRomStorage(std::string_view value) {
		return value == "auto" || value == "file-cache" || value == "flash" || value == "flash-reuse" || value == "psram" || value == "psram-reuse";
	}

	SettingsSection sectionFor(std::string_view name) {
		if (name == "general") {
			return SettingsSection::General;
		}
		if (name == "video") {
			return SettingsSection::Video;
		}
		if (name == "palette") {
			return SettingsSection::Palette;
		}
		if (name == "input") {
			return SettingsSection::Input;
		}
		if (name == "audio") {
			return SettingsSection::Audio;
		}
		if (name == "performance") {
			return SettingsSection::Performance;
		}
		if (name == "advanced" || name == "experimental") {
			return SettingsSection::Experimental;
		}
		if (name == "debug") {
			return SettingsSection::Debug;
		}
		return SettingsSection::Unknown;
	}

	bool applySetting(GbEmuSettings& settings, SettingsSection section, std::string_view key, std::string_view value) {
		switch (section) {
			case SettingsSection::General:
				if (key == "rom_dir") {
					return assignFixed(settings.romDirectory, value);
				}
				if (key == "last_rom") {
					return assignFixed(settings.lastRom, value);
				}
				if (key == "profile") {
					if (!(value.empty() || value == "none" || value == "playable")) {
						return false;
					}
					if (!assignFixed(settings.playbackProfile, value)) {
						return false;
					}
					if (value == "playable") {
						return assignFixed(settings.speedMode, "playable");
					}
					return assignFixed(settings.speedMode, "manual");
				}
				return true;
			case SettingsSection::Video:
				if (key == "display") {
					return isKnownDisplay(value) && assignFixed(settings.display, canonicalDisplay(value));
				}
				if (key == "render_every") {
					return parseUnsigned(value, settings.renderEvery) && settings.renderEvery != 0;
				}
				if (key == "sync_realtime") {
					return parseBool(value, settings.syncRealtime);
				}
				return true;
			case SettingsSection::Palette:
				if (key == "name") {
					return isKnownPalette(value) && assignFixed(settings.palette, value);
				}
				return true;
			case SettingsSection::Input:
				if (key == "input_every") {
					return parseUnsigned(value, settings.inputEvery) && settings.inputEvery != 0;
				}
				if (key == "exit_combo") {
					return value == "start+select" || value == "select+start";
				}
				if (key == "exit_hold_ms") {
					return parseUnsigned(value, settings.exitHoldMs);
				}
				return true;
			case SettingsSection::Audio:
				if (key == "enabled") {
					return parseBool(value, settings.audioEnabled);
				}
				return true;
			case SettingsSection::Performance:
				if (key == "rom_flash_reuse") {
					if (!parseBool(value, settings.romFlashReuse)) {
						return false;
					}
					return settings.romFlashReuse ? assignFixed(settings.romStorage, "flash-reuse") : true;
				}
				if (key == "fast_timing") {
					return parseBool(value, settings.fastTiming);
				}
				if (key == "fast_halt") {
					return parseBool(value, settings.fastHalt);
				}
				return true;
			case SettingsSection::Experimental:
				if (key == "speed_mode") {
					if (!isKnownSpeedMode(value) || !assignFixed(settings.speedMode, value)) {
						return false;
					}
					return assignFixed(settings.playbackProfile, value == "playable" ? "playable" : "none");
				}
				if (key == "frames") {
					return parseUnsigned(value, settings.frames);
				}
				if (key == "steps_per_frame") {
					return parseUnsigned(value, settings.stepsPerFrame) && settings.stepsPerFrame != 0;
				}
				if (key == "cycles_per_frame") {
					return parseUnsigned(value, settings.cyclesPerFrame);
				}
				if (key == "warmup") {
					return isKnownWarmupMode(value) && assignFixed(settings.warmupMode, value);
				}
				if (key == "max_cycles") {
					return parseUnsigned(value, settings.maxWarmupCycles) && settings.maxWarmupCycles != 0;
				}
				if (key == "min_tile_data") {
					return parseUnsigned(value, settings.minTileData) && settings.minTileData != 0;
				}
				if (key == "rom_storage") {
					if (!isKnownRomStorage(value) || !assignFixed(settings.romStorage, value)) {
						return false;
					}
					settings.romFlashReuse = value == "flash-reuse";
					return true;
				}
				if (key == "no_ppu") {
					return parseBool(value, settings.noPpu);
				}
				return true;
			case SettingsSection::Debug:
				if (key == "profile") {
					return parseBool(value, settings.debugProfile);
				}
				if (key == "profile_every") {
					return parseUnsigned(value, settings.profileEvery) && settings.profileEvery != 0;
				}
				if (key == "fault_trace") {
					return parseBool(value, settings.faultTrace);
				}
				return true;
			case SettingsSection::Unknown:
				return true;
		}
		return true;
	}

	bool ensureDirectory(std::string_view path) {
		lexxos::vfs::Stat node{};
		if (lexxos::vfs::stat(path, node)) {
			return node.type == lexxos::vfs::NodeType::Directory;
		}
		auto const status = lexxos::vfs::createDirectory(path);
		return status == lexxos::vfs::WriteStatus::Ok || status == lexxos::vfs::WriteStatus::IsDirectory;
	}

	bool ensureDefaultDirectories() {
		return ensureDirectory(gbEmuSettingsRootDirectory) && ensureDirectory(gbEmuSettingsDirectory);
	}

} // namespace

std::string_view GbEmuSettings::romDirectoryPath() const {
	return fixedView(romDirectory);
}

std::string_view GbEmuSettings::lastRomPath() const {
	return fixedView(lastRom);
}

std::string_view GbEmuSettings::playbackProfileName() const {
	return fixedView(playbackProfile);
}

std::string_view GbEmuSettings::displayName() const {
	return fixedView(display);
}

std::string_view GbEmuSettings::paletteName() const {
	return fixedView(palette);
}

std::string_view GbEmuSettings::speedModeName() const {
	return fixedView(speedMode);
}

std::string_view GbEmuSettings::warmupModeName() const {
	return fixedView(warmupMode);
}

std::string_view GbEmuSettings::romStorageName() const {
	return fixedView(romStorage);
}

GbEmuSettings defaultGbEmuSettings() {
	GbEmuSettings settings{};
	(void) assignFixed(settings.romDirectory, "/mnt/sd/gb");
	(void) assignFixed(settings.lastRom, "");
	(void) assignFixed(settings.playbackProfile, "playable");
	(void) assignFixed(settings.display, "crop");
	(void) assignFixed(settings.palette, "dmg_green");
	(void) assignFixed(settings.speedMode, "playable");
	(void) assignFixed(settings.warmupMode, "off");
	(void) assignFixed(settings.romStorage, "auto");
	settings.renderEvery = 4;
	settings.syncRealtime = true;
	settings.inputEvery = 2;
	settings.exitHoldMs = 1000;
#if defined(LEXXOS_KERNEL_ESPIDF) || defined(LEXXOS_PLATFORM_ESPIDF)
	settings.audioEnabled = true;
	settings.romFlashReuse = true;
#else
	settings.audioEnabled = false;
	settings.romFlashReuse = false;
#endif
	settings.fastTiming = true;
	settings.fastHalt = true;
	settings.frames = 16384;
	settings.stepsPerFrame = 512;
	settings.cyclesPerFrame = 0;
	settings.maxWarmupCycles = 8988672;
	settings.minTileData = 256;
	settings.noPpu = false;
	settings.debugProfile = false;
	settings.profileEvery = 120;
	settings.faultTrace = false;
	return settings;
}

bool parseGbEmuSettings(std::string_view text, GbEmuSettings& settings) {
	settings = defaultGbEmuSettings();
	auto section = SettingsSection::Unknown;
	bool sawAssignment = false;

	while (!text.empty()) {
		auto const lineEnd = text.find('\n');
		auto line = lineEnd == std::string_view::npos ? text : text.substr(0, lineEnd);
		text = lineEnd == std::string_view::npos ? std::string_view{} : text.substr(lineEnd + 1);

		auto const comment = line.find_first_of("#;");
		if (comment != std::string_view::npos) {
			line = line.substr(0, comment);
		}
		line = trim(line);
		if (line.empty()) {
			continue;
		}

		if (line.front() == '[' && line.back() == ']') {
			section = sectionFor(trim(line.substr(1, line.size() - 2)));
			continue;
		}

		auto const separator = line.find('=');
		if (separator == std::string_view::npos) {
			return false;
		}
		auto const key = trim(line.substr(0, separator));
		auto const value = trim(line.substr(separator + 1));
		if (key.empty() || !applySetting(settings, section, key, value)) {
			return false;
		}
		sawAssignment = true;
	}

	return sawAssignment;
}

bool setGbEmuRomDirectory(GbEmuSettings& settings, std::string_view value) {
	return assignFixed(settings.romDirectory, value);
}

bool setGbEmuLastRom(GbEmuSettings& settings, std::string_view value) {
	return assignFixed(settings.lastRom, value);
}

GbEmuSettingsLoadResult loadOrCreateGbEmuSettings() {
	GbEmuSettingsLoadResult result{};
	result.settings = defaultGbEmuSettings();

	lexxos::vfs::Stat stat{};
	if (!lexxos::vfs::stat(gbEmuSettingsPath, stat) || stat.type == lexxos::vfs::NodeType::Missing) {
		if (!ensureDefaultDirectories()) {
			result.status = GbEmuSettingsStatus::StorageUnavailable;
			return result;
		}
		result.writeStatus = lexxos::vfs::writeText(
		    gbEmuSettingsPath,
		    gbEmuDefaultSettingsContent,
		    lexxos::vfs::WriteMode::CreateTruncate);
		if (result.writeStatus != lexxos::vfs::WriteStatus::Ok) {
			result.status = GbEmuSettingsStatus::WriteFailed;
			return result;
		}
		result.syncStatus = lexxos::vfs::sync();
		if (result.syncStatus != lexxos::vfs::SyncStatus::Ok) {
			result.status = GbEmuSettingsStatus::SyncFailed;
			return result;
		}
		result.status = GbEmuSettingsStatus::CreatedDefault;
		return result;
	}

	if (stat.type != lexxos::vfs::NodeType::RegularFile || stat.sizeBytes > maxSettingsFileBytes) {
		result.status = GbEmuSettingsStatus::InvalidFile;
		return result;
	}

	lexxos::vfs::FileHandle file;
	result.openStatus = lexxos::vfs::open(gbEmuSettingsPath, lexxos::vfs::OpenMode::Read, file);
	if (result.openStatus != lexxos::vfs::OpenStatus::Ok) {
		result.status = GbEmuSettingsStatus::ReadFailed;
		return result;
	}

	auto& buffer = settingsIoWorkspace().buffer;
	std::memset(buffer.data(), 0, buffer.size());
	std::size_t bytesRead = 0;
	result.readStatus = file.read(buffer.data(), static_cast<std::size_t>(stat.sizeBytes), bytesRead);
	if (result.readStatus != lexxos::vfs::ReadStatus::Ok || bytesRead != static_cast<std::size_t>(stat.sizeBytes)) {
		result.status = GbEmuSettingsStatus::ReadFailed;
		return result;
	}

	if (!parseGbEmuSettings(std::string_view{buffer.data(), bytesRead}, result.settings)) {
		result.settings = defaultGbEmuSettings();
		result.status = GbEmuSettingsStatus::InvalidFile;
		return result;
	}

	result.status = GbEmuSettingsStatus::Loaded;
	return result;
}

GbEmuSettingsSaveResult saveGbEmuSettings(GbEmuSettings const& settings) {
	GbEmuSettingsSaveResult result{};
	if (!ensureDefaultDirectories()) {
		result.status = GbEmuSettingsStatus::StorageUnavailable;
		return result;
	}

	auto& content = settingsIoWorkspace().buffer;
	std::memset(content.data(), 0, content.size());
	std::size_t position = 0;
	if (!appendSettingsContent(settings, content, position)) {
		result.status = GbEmuSettingsStatus::WriteFailed;
		result.writeStatus = lexxos::vfs::WriteStatus::TooLarge;
		return result;
	}

	result.writeStatus = lexxos::vfs::writeText(
	    gbEmuSettingsPath,
	    std::string_view{content.data(), position},
	    lexxos::vfs::WriteMode::CreateTruncate);
	if (result.writeStatus != lexxos::vfs::WriteStatus::Ok) {
		result.status = GbEmuSettingsStatus::WriteFailed;
		return result;
	}

	result.syncStatus = lexxos::vfs::sync();
	if (result.syncStatus != lexxos::vfs::SyncStatus::Ok) {
		result.status = GbEmuSettingsStatus::SyncFailed;
		return result;
	}

	result.status = GbEmuSettingsStatus::Loaded;
	return result;
}

} // namespace lexxos::apps::gameboy_programs::detail
