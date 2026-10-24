#include <lexxos/apps/gameboy/detail/gameboy_launcher.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <lexxos/app/console_mirror.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>
#include <lexxos/apps/gameboy/detail/gameboy_settings.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/system.h>
#include <lexxos/program/arguments.h>
#include <lexxos/tui/tui.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::gameboy_programs {
namespace {

	constexpr std::size_t maxLauncherRoms = 64;
	constexpr std::size_t maxLineChars = 96;

	struct FixedLine {
		std::array<char, maxLineChars> data{};
		std::size_t size{0};

		void clear() {
			size = 0;
			data[0] = '\0';
		}

		bool append(std::string_view text) {
			auto const available = data.size() - size;
			if (available == 0) {
				return false;
			}
			auto count = text.size();
			if (count >= available) {
				count = available - 1U;
			}
			if (count != 0) {
				std::memcpy(data.data() + size, text.data(), count);
				size += count;
			}
			data[size] = '\0';
			return count == text.size();
		}

		bool appendUnsigned(std::uint32_t value) {
			char digits[10]{};
			std::size_t count = 0;
			do {
				digits[count++] = static_cast<char>('0' + (value % 10U));
				value /= 10U;
			} while (value != 0 && count < sizeof(digits));
			for (std::size_t index = 0; index < count; ++index) {
				if (!append(std::string_view{&digits[count - index - 1U], 1})) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] std::string_view view() const {
			return std::string_view{data.data(), size};
		}
	};

	template <std::size_t Size>
	bool copyFixed(std::array<char, Size>& output, std::string_view text) {
		if (text.size() >= output.size()) {
			return false;
		}
		std::memset(output.data(), 0, output.size());
		if (!text.empty()) {
			std::memcpy(output.data(), text.data(), text.size());
		}
		output[text.size()] = '\0';
		return true;
	}

	char lowerAscii(char value) {
		return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
	}

	bool equalsIgnoreCase(std::string_view left, std::string_view right) {
		if (left.size() != right.size()) {
			return false;
		}
		for (std::size_t index = 0; index < left.size(); ++index) {
			if (lowerAscii(left[index]) != lowerAscii(right[index])) {
				return false;
			}
		}
		return true;
	}

	bool lessIgnoreCase(std::string_view left, std::string_view right) {
		auto const count = left.size() < right.size() ? left.size() : right.size();
		for (std::size_t index = 0; index < count; ++index) {
			auto const l = lowerAscii(left[index]);
			auto const r = lowerAscii(right[index]);
			if (l < r) {
				return true;
			}
			if (l > r) {
				return false;
			}
		}
		return left.size() < right.size();
	}

	bool hasGameBoyExtension(std::string_view name) {
		if (name.empty() || name.front() == '.') {
			return false;
		}
		auto const dot = name.rfind('.');
		if (dot == std::string_view::npos) {
			return false;
		}
		auto const extension = name.substr(dot);
		return equalsIgnoreCase(extension, ".gb") || equalsIgnoreCase(extension, ".gbc");
	}

	struct RomEntry {
		std::array<char, lexxos::vfs::maxNameLength> name{};
		std::array<char, lexxos::vfs::maxPathLength> path{};
	};

	struct RomList {
		std::array<RomEntry, maxLauncherRoms> entries{};
		std::size_t count{0};
		bool overflow{false};
		bool directoryMissing{false};
		bool notDirectory{false};
		bool listFailed{false};
	};

	struct LauncherWorkspace {
		RomList roms{};
		std::array<char, lexxos::vfs::maxPathLength> launchPath{};
		lexxos::program::ParsedArguments launchArguments{};
	};

	LauncherWorkspace& launcherWorkspace() {
		static LauncherWorkspace workspace;
		return workspace;
	}

	void clearRomList(RomList& roms) {
		roms.count = 0;
		roms.overflow = false;
		roms.directoryMissing = false;
		roms.notDirectory = false;
		roms.listFailed = false;
	}

	std::string_view entryName(RomEntry const& entry) {
		return std::string_view{entry.name.data()};
	}

	std::string_view entryPath(RomEntry const& entry) {
		return std::string_view{entry.path.data()};
	}

	bool insertRom(RomList& roms, std::string_view directory, lexxos::vfs::DirectoryEntry const& entry) {
		if (roms.count >= roms.entries.size()) {
			roms.overflow = true;
			return true;
		}

		std::array<char, lexxos::vfs::maxPathLength> path{};
		if (!joinPath(directory, entry.name, path)) {
			roms.overflow = true;
			return true;
		}

		std::size_t index = roms.count;
		while (index > 0 && lessIgnoreCase(entry.name, entryName(roms.entries[index - 1U]))) {
			roms.entries[index] = roms.entries[index - 1U];
			--index;
		}

		if (!copyFixed(roms.entries[index].name, entry.name) || !copyFixed(roms.entries[index].path, path.data())) {
			roms.overflow = true;
			return true;
		}
		++roms.count;
		return true;
	}

	struct RomScanContext {
		RomList* roms{nullptr};
		std::string_view directory{};
	};

	bool visitRomEntry(lexxos::vfs::DirectoryEntry const& entry, void* userData) {
		auto& scan = *static_cast<RomScanContext*>(userData);
		if (scan.roms == nullptr || entry.type != lexxos::vfs::NodeType::RegularFile || !hasGameBoyExtension(entry.name)) {
			return true;
		}
		return insertRom(*scan.roms, scan.directory, entry);
	}

	void scanRoms(std::string_view directory, RomList& roms) {
		clearRomList(roms);
		lexxos::vfs::Stat stat{};
		if (!lexxos::vfs::stat(directory, stat) || stat.type == lexxos::vfs::NodeType::Missing) {
			roms.directoryMissing = true;
			return;
		}
		if (stat.type != lexxos::vfs::NodeType::Directory) {
			roms.notDirectory = true;
			return;
		}

		RomScanContext scan{.roms = &roms, .directory = directory};
		roms.listFailed = !lexxos::vfs::list(directory, visitRomEntry, &scan);
	}

	void selectLastRom(lexxos::tui::MenuState& menu, RomList const& roms, std::string_view lastRom) {
		if (roms.count == 0 || lastRom.empty()) {
			menu.clamp(roms.count, 1);
			return;
		}
		for (std::size_t index = 0; index < roms.count; ++index) {
			if (entryPath(roms.entries[index]) == lastRom) {
				menu.selected = index;
				menu.clamp(roms.count, 1);
				return;
			}
		}
	}

	enum class SettingItem : std::uint8_t {
		Display,
		Palette,
		RenderEvery,
		InputEvery,
		SyncRealtime,
		Audio,
		AdvancedHeader,
		SpeedMode,
		Frames,
		StepsPerFrame,
		CyclesPerFrame,
		WarmupMode,
		MaxWarmupCycles,
		MinTileData,
		RomStorage,
		NoPpu,
		FastTiming,
		FastHalt,
		DebugProfile,
		ProfileEvery,
		FaultTrace,
		Back,
	};

	constexpr std::size_t settingItemCount = 22;
	constexpr std::string_view displayOptions[] = {"crop", "fit", "1x", "none"};
	constexpr std::string_view paletteOptions[] = {
	    "dmg_green",
	    "default",
	    "pocket_gray",
	    "gbc_blue",
	    "gbc_brown",
	    "gbc_red",
	    "gbc_pastel",
	    "high_contrast",
	};
	constexpr std::uint32_t renderEveryOptions[] = {1, 2, 3, 4, 5, 6, 8};
	constexpr std::uint32_t inputEveryOptions[] = {1, 2, 3, 4, 5};
	constexpr std::string_view speedModeOptions[] = {"playable", "playable-fast", "max-speed", "psram-fast", "manual"};
	constexpr std::uint32_t frameOptions[] = {0, 60, 300, 600, 1024, 4096, 8192, 16384};
	constexpr std::uint32_t stepsPerFrameOptions[] = {256, 512, 1024, 2048, 4096, 8192, 16384, 32768};
	constexpr std::uint32_t cyclesPerFrameOptions[] = {0, 17556, 35112, defaultGbCyclesPerFrame, defaultGbCyclesPerFrame * 2U, maxGbCyclesPerFrame};
	constexpr std::string_view warmupModeOptions[] = {"off", "vram", "visible", "tile-data", "bg-ready"};
	constexpr std::uint32_t maxWarmupCycleOptions[] = {defaultGbCyclesPerFrame, defaultGbCyclesPerFrame * 32U, defaultGbEmuWarmupCycles, defaultGbCyclesPerFrame * 256U, maxGbEmuWarmupCycles};
	constexpr std::uint32_t minTileDataOptions[] = {1, 64, 128, 256, 512, 1024};
	constexpr std::string_view romStorageOptions[] = {"auto", "file-cache", "flash-reuse", "psram-reuse", "flash", "psram"};
	constexpr std::uint32_t profileEveryOptions[] = {30, 60, 120, 240};

	template <typename T, std::size_t Size>
	std::size_t optionIndex(T const (&values)[Size], T value) {
		for (std::size_t index = 0; index < Size; ++index) {
			if (values[index] == value) {
				return index;
			}
		}
		return 0;
	}

	template <typename T, std::size_t Size>
	T cycleOption(T const (&values)[Size], T value, int direction) {
		auto index = optionIndex(values, value);
		if (direction < 0) {
			index = index == 0 ? Size - 1U : index - 1U;
		} else {
			index = (index + 1U) % Size;
		}
		return values[index];
	}

	std::string_view boolText(bool value) {
		return value ? "on" : "off";
	}

	SettingItem settingItemAt(std::size_t index) {
		return static_cast<SettingItem>(index);
	}

	std::string_view settingLabel(SettingItem item) {
		switch (item) {
			case SettingItem::Display:
				return "Display";
			case SettingItem::Palette:
				return "Palette";
			case SettingItem::RenderEvery:
				return "Render every";
			case SettingItem::InputEvery:
				return "Input every";
			case SettingItem::SyncRealtime:
				return "Sync realtime";
			case SettingItem::Audio:
				return "Audio";
			case SettingItem::AdvancedHeader:
				return "Advanced / experimental";
			case SettingItem::SpeedMode:
				return "Speed mode";
			case SettingItem::Frames:
				return "Frames";
			case SettingItem::StepsPerFrame:
				return "Steps/frame";
			case SettingItem::CyclesPerFrame:
				return "Cycles/frame";
			case SettingItem::WarmupMode:
				return "Warmup";
			case SettingItem::MaxWarmupCycles:
				return "Warmup cycles";
			case SettingItem::MinTileData:
				return "Min tile data";
			case SettingItem::RomStorage:
				return "ROM storage";
			case SettingItem::NoPpu:
				return "No PPU";
			case SettingItem::FastTiming:
				return "Fast timing";
			case SettingItem::FastHalt:
				return "Fast halt";
			case SettingItem::DebugProfile:
				return "Debug profile";
			case SettingItem::ProfileEvery:
				return "Profile every";
			case SettingItem::FaultTrace:
				return "Fault trace";
			case SettingItem::Back:
				return "Back";
		}
		return "";
	}

	void appendSettingValue(FixedLine& line, detail::GbEmuSettings const& settings, SettingItem item) {
		switch (item) {
			case SettingItem::Display:
				line.append(settings.displayName());
				return;
			case SettingItem::Palette:
				line.append(settings.paletteName());
				return;
			case SettingItem::RenderEvery:
				line.appendUnsigned(settings.renderEvery);
				return;
			case SettingItem::InputEvery:
				line.appendUnsigned(settings.inputEvery);
				return;
			case SettingItem::SyncRealtime:
				line.append(boolText(settings.syncRealtime));
				return;
			case SettingItem::Audio:
				line.append(boolText(settings.audioEnabled));
				return;
			case SettingItem::AdvancedHeader:
				return;
			case SettingItem::SpeedMode:
				line.append(settings.speedModeName());
				return;
			case SettingItem::Frames:
				if (settings.frames == 0) {
					line.append("unlimited");
				} else {
					line.appendUnsigned(settings.frames);
				}
				return;
			case SettingItem::StepsPerFrame:
				line.appendUnsigned(settings.stepsPerFrame);
				return;
			case SettingItem::CyclesPerFrame:
				line.appendUnsigned(settings.cyclesPerFrame);
				return;
			case SettingItem::WarmupMode:
				line.append(settings.warmupModeName());
				return;
			case SettingItem::MaxWarmupCycles:
				line.appendUnsigned(settings.maxWarmupCycles);
				return;
			case SettingItem::MinTileData:
				line.appendUnsigned(settings.minTileData);
				return;
			case SettingItem::RomStorage:
				line.append(settings.romStorageName());
				return;
			case SettingItem::NoPpu:
				line.append(boolText(settings.noPpu));
				return;
			case SettingItem::FastTiming:
				line.append(boolText(settings.fastTiming));
				return;
			case SettingItem::FastHalt:
				line.append(boolText(settings.fastHalt));
				return;
			case SettingItem::DebugProfile:
				line.append(boolText(settings.debugProfile));
				return;
			case SettingItem::ProfileEvery:
				line.appendUnsigned(settings.profileEvery);
				return;
			case SettingItem::FaultTrace:
				line.append(boolText(settings.faultTrace));
				return;
			case SettingItem::Back:
				return;
		}
	}

	bool changeSetting(detail::GbEmuSettings& settings, SettingItem item, int direction) {
		switch (item) {
			case SettingItem::Display: {
				auto const value = cycleOption(displayOptions, settings.displayName(), direction);
				return copyFixed(settings.display, value);
			}
			case SettingItem::Palette: {
				auto const value = cycleOption(paletteOptions, settings.paletteName(), direction);
				return copyFixed(settings.palette, value);
			}
			case SettingItem::RenderEvery:
				settings.renderEvery = cycleOption(renderEveryOptions, settings.renderEvery, direction);
				return true;
			case SettingItem::InputEvery:
				settings.inputEvery = cycleOption(inputEveryOptions, settings.inputEvery, direction);
				return true;
			case SettingItem::SyncRealtime:
				settings.syncRealtime = !settings.syncRealtime;
				return true;
			case SettingItem::Audio:
				settings.audioEnabled = !settings.audioEnabled;
				return true;
			case SettingItem::AdvancedHeader:
				return false;
			case SettingItem::SpeedMode: {
				auto const value = cycleOption(speedModeOptions, settings.speedModeName(), direction);
				if (!copyFixed(settings.speedMode, value) || !copyFixed(settings.playbackProfile, value == "playable" ? "playable" : "none")) {
					return false;
				}
				if (value == "manual" && settings.cyclesPerFrame == 0) {
					settings.syncRealtime = false;
				}
				return true;
			}
			case SettingItem::Frames:
				settings.frames = cycleOption(frameOptions, settings.frames, direction);
				return true;
			case SettingItem::StepsPerFrame:
				settings.stepsPerFrame = cycleOption(stepsPerFrameOptions, settings.stepsPerFrame, direction);
				return true;
			case SettingItem::CyclesPerFrame:
				settings.cyclesPerFrame = cycleOption(cyclesPerFrameOptions, settings.cyclesPerFrame, direction);
				return true;
			case SettingItem::WarmupMode: {
				auto const value = cycleOption(warmupModeOptions, settings.warmupModeName(), direction);
				return copyFixed(settings.warmupMode, value);
			}
			case SettingItem::MaxWarmupCycles:
				settings.maxWarmupCycles = cycleOption(maxWarmupCycleOptions, settings.maxWarmupCycles, direction);
				return true;
			case SettingItem::MinTileData:
				settings.minTileData = cycleOption(minTileDataOptions, settings.minTileData, direction);
				return true;
			case SettingItem::RomStorage: {
				auto const value = cycleOption(romStorageOptions, settings.romStorageName(), direction);
				if (!copyFixed(settings.romStorage, value)) {
					return false;
				}
				settings.romFlashReuse = value == "flash-reuse";
				return true;
			}
			case SettingItem::NoPpu:
				settings.noPpu = !settings.noPpu;
				return true;
			case SettingItem::FastTiming:
				settings.fastTiming = !settings.fastTiming;
				return true;
			case SettingItem::FastHalt:
				settings.fastHalt = !settings.fastHalt;
				return true;
			case SettingItem::DebugProfile:
				settings.debugProfile = !settings.debugProfile;
				return true;
			case SettingItem::ProfileEvery:
				settings.profileEvery = cycleOption(profileEveryOptions, settings.profileEvery, direction);
				return true;
			case SettingItem::FaultTrace:
				settings.faultTrace = !settings.faultTrace;
				return true;
			case SettingItem::Back:
				return false;
		}
		return false;
	}

	bool prepareLaunchArguments(
	    std::string_view path,
	    detail::GbEmuSettings const& settings,
	    lexxos::program::ParsedArguments& launchArguments) {
		launchArguments = lexxos::program::ParsedArguments{};
		if (!launchArguments.addPositional("rom", path)) {
			return false;
		}
		if (settings.audioEnabled && !launchArguments.setNamed("audio", "", true)) {
			return false;
		}
		return true;
	}

	void saveStatus(FixedLine& status, detail::GbEmuSettingsSaveResult const& result) {
		status.clear();
		if (result.ok()) {
			status.append("Settings saved");
			return;
		}
		status.append("Save failed: ");
		switch (result.status) {
			case detail::GbEmuSettingsStatus::StorageUnavailable:
				status.append("storage unavailable");
				return;
			case detail::GbEmuSettingsStatus::WriteFailed:
				status.append(lexxos::vfs::toString(result.writeStatus));
				return;
			case detail::GbEmuSettingsStatus::SyncFailed:
				status.append(lexxos::vfs::toString(result.syncStatus));
				return;
			default:
				status.append("unknown");
				return;
		}
	}

	void loadStatus(FixedLine& status, detail::GbEmuSettingsLoadResult const& result) {
		status.clear();
		switch (result.status) {
			case detail::GbEmuSettingsStatus::Loaded:
				status.append("A run  B back  Start settings  Select exit");
				return;
			case detail::GbEmuSettingsStatus::CreatedDefault:
				status.append("Created ");
				status.append(detail::gbEmuSettingsPath);
				return;
			case detail::GbEmuSettingsStatus::StorageUnavailable:
				status.append("Using defaults: SD settings unavailable");
				return;
			case detail::GbEmuSettingsStatus::InvalidFile:
				status.append("Invalid settings, using defaults");
				return;
			case detail::GbEmuSettingsStatus::ReadFailed:
				status.append("Cannot read settings, using defaults");
				return;
			case detail::GbEmuSettingsStatus::WriteFailed:
				status.append("Cannot create default settings");
				return;
			case detail::GbEmuSettingsStatus::SyncFailed:
				status.append("Cannot sync default settings");
				return;
		}
	}

	void romStatus(FixedLine& status, RomList const& roms, std::string_view directory) {
		if (roms.directoryMissing) {
			status.clear();
			status.append("ROM dir missing: ");
			status.append(directory);
		} else if (roms.notDirectory) {
			status.clear();
			status.append("ROM path is not a directory");
		} else if (roms.listFailed) {
			status.clear();
			status.append("Cannot list ROM directory");
		} else if (roms.count == 0) {
			status.clear();
			status.append("No .gb/.gbc ROMs in ");
			status.append(directory);
		} else if (roms.overflow) {
			status.clear();
			status.append("ROM list truncated");
		}
	}

	void renderHeader(lexxos::tui::Screen& screen, lexxos::tui::Palette const& palette, std::string_view subtitle) {
		screen.drawText(1, 1, "LexxOS Game Boy", {.foreground = palette.accent, .background = palette.background});
		screen.drawText(1, 2, subtitle, {.foreground = palette.muted, .background = palette.background}, true);
	}

	void renderRomLauncher(
	    lexxos::tui::Screen& screen,
	    lexxos::tui::Palette const& palette,
	    detail::GbEmuSettings const& settings,
	    RomList const& roms,
	    lexxos::tui::MenuState& menu,
	    FixedLine const& status) {
		screen.beginFrame();
		screen.clear(palette.background);
		renderHeader(screen, palette, settings.romDirectoryPath());

		auto const firstRow = 4;
		auto const footerRows = 3;
		auto visibleRows = screen.rows() > firstRow + footerRows ? static_cast<std::size_t>(screen.rows() - firstRow - footerRows) : std::size_t{1};
		menu.clamp(roms.count, visibleRows);

		if (roms.count == 0) {
			screen.drawText(1, firstRow, "No ROMs found", {.foreground = palette.foreground, .background = palette.background});
		} else {
			for (std::size_t row = 0; row < visibleRows; ++row) {
				auto const index = menu.scroll + row;
				if (index >= roms.count) {
					break;
				}
				FixedLine line;
				line.append(index == menu.selected ? "> " : "  ");
				line.append(entryName(roms.entries[index]));
				screen.drawMenuItem(firstRow + static_cast<int>(row), line.view(), index == menu.selected, palette);
			}
		}

		FixedLine countLine;
		countLine.append("ROMs: ");
		countLine.appendUnsigned(static_cast<std::uint32_t>(roms.count));
		if (roms.overflow) {
			countLine.append("+");
		}
		screen.drawText(1, screen.rows() - 3, countLine.view(), {.foreground = palette.muted, .background = palette.background}, true);
		screen.drawText(1, screen.rows() - 2, status.view(), {.foreground = palette.muted, .background = palette.background}, true);
		screen.endFrame();
	}

	void renderSettings(
	    lexxos::tui::Screen& screen,
	    lexxos::tui::Palette const& palette,
	    detail::GbEmuSettings const& settings,
	    lexxos::tui::MenuState& menu,
	    FixedLine const& status) {
		screen.beginFrame();
		screen.clear(palette.background);
		renderHeader(screen, palette, "Settings");

		auto const firstRow = 4;
		auto const footerRows = 3;
		auto visibleRows = screen.rows() > firstRow + footerRows ? static_cast<std::size_t>(screen.rows() - firstRow - footerRows) : std::size_t{1};
		menu.clamp(settingItemCount, visibleRows);

		for (std::size_t row = 0; row < visibleRows; ++row) {
			auto const index = menu.scroll + row;
			if (index >= settingItemCount) {
				break;
			}
			auto const item = settingItemAt(index);
			FixedLine line;
			line.append(index == menu.selected ? "> " : "  ");
			line.append(settingLabel(item));
			if (item != SettingItem::Back && item != SettingItem::AdvancedHeader) {
				line.append(": ");
				appendSettingValue(line, settings, item);
			}
			screen.drawMenuItem(firstRow + static_cast<int>(row), line.view(), index == menu.selected, palette);
		}

		screen.drawText(1, screen.rows() - 3, "Left/right edit  B back", {.foreground = palette.muted, .background = palette.background}, true);
		screen.drawText(1, screen.rows() - 2, status.view(), {.foreground = palette.muted, .background = palette.background}, true);
		screen.endFrame();
	}

	bool waitForInteractiveFocus(lexxos::os::AppContext& context) {
		while (context.processId() != 0 && !context.hasProgramFocus()) {
			if (lexxos::kernel::isShutdownRequested()) {
				return false;
			}
			sleepMs(context, 10);
		}
		return !lexxos::kernel::isShutdownRequested();
	}

	bool acquireLauncherLocks(
	    lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice& display,
	    lexxos::devices::GamepadDevice& gamepad,
	    lexxos::devices::DeviceResourceLock& displayLock,
	    lexxos::devices::DeviceResourceLock& gamepadLock) {
		while (!lexxos::kernel::isShutdownRequested()) {
			if (!waitForInteractiveFocus(context)) {
				return false;
			}
			if (!displayLock) {
				displayLock = context.devices().lockDisplayDevice(
				    display.name(),
				    lexxos::devices::DeviceResourcePriority::Interactive,
				    "gbemu-launcher");
			}
			if (displayLock && !gamepadLock) {
				gamepadLock = context.devices().lockGamepadDevice(
				    gamepad.name(),
				    lexxos::devices::DeviceResourcePriority::Interactive,
				    "gbemu-launcher");
				if (!gamepadLock) {
					displayLock.release();
				}
			}
			if (displayLock && gamepadLock) {
				return true;
			}
			if (context.pollCancellationRequest()) {
				return false;
			}
			sleepMs(context, 5);
		}
		return false;
	}

	int runSelectedRom(
	    lexxos::os::AppContext& context,
	    std::string_view path,
	    detail::GbEmuSettings const& settings,
	    lexxos::program::ParsedArguments& launchArguments,
	    FixedLine& status) {
		if (!prepareLaunchArguments(path, settings, launchArguments)) {
			status.clear();
			status.append("Launch failed: arguments");
			return 2;
		}
		std::array<char, 512> output{};
		lexxos::io::FixedBufferOutputStream capture{output.data(), output.size()};
		auto launchContext = context.createChild({
		    .standardOutput = &capture,
		    .standardError = &capture,
		});
		auto const exitCode = runGameBoyEmulator(launchContext, launchArguments);
		if (exitCode == 0) {
			return exitCode;
		}

		auto const text = capture.view();
		std::string_view lastLine{};
		std::size_t lineEnd = text.size();
		while (lineEnd != 0) {
			while (lineEnd != 0 && (text[lineEnd - 1U] == '\r' || text[lineEnd - 1U] == '\n')) {
				--lineEnd;
			}
			auto lineBegin = lineEnd;
			while (lineBegin != 0 && text[lineBegin - 1U] != '\r' && text[lineBegin - 1U] != '\n') {
				--lineBegin;
			}
			if (lineBegin < lineEnd) {
				lastLine = text.substr(lineBegin, lineEnd - lineBegin);
				break;
			}
			lineEnd = lineBegin;
		}
		if (lastLine.substr(0, 7) == "gbemu: ") {
			lastLine.remove_prefix(7);
		}

		status.clear();
		status.append("Launch failed: ");
		if (!lastLine.empty()) {
			status.append(lastLine);
		} else {
			status.append("exit ");
			status.appendUnsigned(static_cast<std::uint32_t>(exitCode < 0 ? 0 : exitCode));
		}
		return exitCode;
	}

} // namespace

int runGameBoyLauncher(lexxos::os::AppContext& context) {
	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr) {
		context.writeErrorLine("gbemu: display device is not registered");
		return 1;
	}
	if (!display->isReady()) {
		context.writeErrorLine("gbemu: display device is not ready");
		return 1;
	}
	if (display->nativeFormat() != lexxos::devices::PixelFormat::Rgb565) {
		context.writeErrorLine("gbemu: only rgb565 displays are supported by the launcher");
		return 1;
	}

	auto* gamepad = context.devices().primaryGamepadDevice();
	if (gamepad == nullptr) {
		context.writeErrorLine("gbemu: gamepad device is not registered");
		return 1;
	}
	if (!gamepad->isReady()) {
		context.writeErrorLine("gbemu: gamepad device is not ready");
		return 1;
	}

	auto const settingsResult = detail::loadOrCreateGbEmuSettings();
	auto settings = settingsResult.settings;
	FixedLine status;
	loadStatus(status, settingsResult);

	auto& workspace = launcherWorkspace();
	auto& roms = workspace.roms;
	scanRoms(settings.romDirectoryPath(), roms);
	romStatus(status, roms, settings.romDirectoryPath());

	lexxos::devices::DeviceResourceLock displayLock;
	lexxos::devices::DeviceResourceLock gamepadLock;
	if (!acquireLauncherLocks(context, *display, *gamepad, displayLock, gamepadLock)) {
		return context.cancellationRequested() ? 130 : 1;
	}

	auto graphicsMode = lexxos::app::ScopedConsoleMirrorMode{lexxos::app::ConsoleMirrorMode::Graphics};
	lexxos::tui::Screen screen{*display};
	lexxos::tui::InputReader input{gamepad};
	lexxos::tui::Palette palette{};
	lexxos::tui::MenuState romMenu{};
	lexxos::tui::MenuState settingsMenu{};
	selectLastRom(romMenu, roms, settings.lastRomPath());
	input.reset();

	enum class View : std::uint8_t {
		Roms,
		Settings,
	};
	auto view = View::Roms;
	bool redraw = true;
	bool running = true;
	int lastLaunchExitCode = 0;

	while (running && !lexxos::kernel::isShutdownRequested()) {
		if (context.pollCancellationRequest()) {
			return 130;
		}

		if (!displayLock || !gamepadLock) {
			if (!acquireLauncherLocks(context, *display, *gamepad, displayLock, gamepadLock)) {
				return context.cancellationRequested() ? 130 : 1;
			}
			input.reset();
			redraw = true;
		}

		if (redraw) {
			if (view == View::Roms) {
				renderRomLauncher(screen, palette, settings, roms, romMenu, status);
			} else {
				renderSettings(screen, palette, settings, settingsMenu, status);
			}
			redraw = false;
		}

		lexxos::tui::Action action = lexxos::tui::Action::None;
		if (!input.poll(action)) {
			sleepMs(context, 10);
			continue;
		}

		if (view == View::Roms) {
			auto const visibleRows = screen.rows() > 7 ? static_cast<std::size_t>(screen.rows() - 7) : std::size_t{1};
			switch (action) {
				case lexxos::tui::Action::Up:
					redraw = romMenu.moveUp(roms.count, visibleRows);
					break;
				case lexxos::tui::Action::Down:
					redraw = romMenu.moveDown(roms.count, visibleRows);
					break;
				case lexxos::tui::Action::Confirm:
					if (roms.count != 0 && romMenu.selected < roms.count) {
						auto& launchPath = workspace.launchPath;
						copyFixed(launchPath, entryPath(roms.entries[romMenu.selected]));
						(void) detail::setGbEmuLastRom(settings, launchPath.data());
						saveStatus(status, detail::saveGbEmuSettings(settings));
						renderRomLauncher(screen, palette, settings, roms, romMenu, status);

						displayLock.release();
						gamepadLock.release();
						lastLaunchExitCode = runSelectedRom(context, launchPath.data(), settings, workspace.launchArguments, status);
						lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Graphics);

						if (!acquireLauncherLocks(context, *display, *gamepad, displayLock, gamepadLock)) {
							return context.cancellationRequested() ? 130 : lastLaunchExitCode;
						}
						input.reset();
						scanRoms(settings.romDirectoryPath(), roms);
						if (lastLaunchExitCode == 0) {
							status.clear();
							status.append("Last exit: 0");
						}
						romStatus(status, roms, settings.romDirectoryPath());
						selectLastRom(romMenu, roms, settings.lastRomPath());
						redraw = true;
					}
					break;
				case lexxos::tui::Action::Start:
					status.clear();
					status.append("Left/right edit  B back");
					view = View::Settings;
					redraw = true;
					break;
				case lexxos::tui::Action::Select:
				case lexxos::tui::Action::Back:
					running = false;
					break;
				case lexxos::tui::Action::Left:
				case lexxos::tui::Action::Right:
				case lexxos::tui::Action::None:
					break;
			}
		} else {
			auto const visibleRows = screen.rows() > 7 ? static_cast<std::size_t>(screen.rows() - 7) : std::size_t{1};
			auto const selectedItem = settingItemAt(settingsMenu.selected);
			switch (action) {
				case lexxos::tui::Action::Up:
					redraw = settingsMenu.moveUp(settingItemCount, visibleRows);
					break;
				case lexxos::tui::Action::Down:
					redraw = settingsMenu.moveDown(settingItemCount, visibleRows);
					break;
				case lexxos::tui::Action::Left:
				case lexxos::tui::Action::Right:
				case lexxos::tui::Action::Confirm:
					if (selectedItem == SettingItem::Back) {
						view = View::Roms;
						redraw = true;
						break;
					}
					if (changeSetting(settings, selectedItem, action == lexxos::tui::Action::Left ? -1 : 1)) {
						saveStatus(status, detail::saveGbEmuSettings(settings));
						if (selectedItem == SettingItem::Display || selectedItem == SettingItem::Palette || selectedItem == SettingItem::RenderEvery || selectedItem == SettingItem::InputEvery) {
							scanRoms(settings.romDirectoryPath(), roms);
						}
						redraw = true;
					}
					break;
				case lexxos::tui::Action::Back:
				case lexxos::tui::Action::Select:
				case lexxos::tui::Action::Start:
					status.clear();
					status.append("A run  B back  Start settings  Select exit");
					romStatus(status, roms, settings.romDirectoryPath());
					view = View::Roms;
					redraw = true;
					break;
				case lexxos::tui::Action::None:
					break;
			}
		}
	}

	return 0;
}

} // namespace lexxos::apps::gameboy_programs
