#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::gameboy_programs::detail {

inline constexpr std::string_view gbEmuSettingsPath = "/mnt/sd/apps/gbemu/settings.ini";
inline constexpr std::string_view gbEmuSettingsDirectory = "/mnt/sd/apps/gbemu";
inline constexpr std::string_view gbEmuSettingsRootDirectory = "/mnt/sd/apps";

inline constexpr std::string_view gbEmuDefaultSettingsContent =
    "[general]\n"
    "rom_dir = /mnt/sd/gb\n"
    "last_rom = \n"
    "profile = playable\n"
    "\n"
    "[video]\n"
    "display = crop\n"
    "render_every = 4\n"
    "sync_realtime = true\n"
    "\n"
    "[palette]\n"
    "name = dmg_green\n"
    "\n"
    "[input]\n"
    "input_every = 2\n"
    "exit_combo = start+select\n"
    "exit_hold_ms = 1000\n"
    "\n"
    "[audio]\n"
    "enabled = "
#if defined(LEXXOS_KERNEL_ESPIDF) || defined(LEXXOS_PLATFORM_ESPIDF)
    "true\n"
#else
    "false\n"
#endif
    "\n"
    "[performance]\n"
    "rom_flash_reuse = "
#if defined(LEXXOS_KERNEL_ESPIDF) || defined(LEXXOS_PLATFORM_ESPIDF)
    "true\n"
#else
    "false\n"
#endif
    "fast_timing = true\n"
    "fast_halt = true\n"
    "\n"
    "[experimental]\n"
    "speed_mode = playable\n"
    "frames = 16384\n"
    "steps_per_frame = 512\n"
    "cycles_per_frame = 0\n"
    "warmup = off\n"
    "max_cycles = 8988672\n"
    "min_tile_data = 256\n"
    "rom_storage = auto\n"
    "no_ppu = false\n"
    "\n"
    "[debug]\n"
    "profile = false\n"
    "profile_every = 120\n"
    "fault_trace = false\n";

struct GbEmuSettings {
	std::array<char, lexxos::vfs::maxPathLength> romDirectory{};
	std::array<char, lexxos::vfs::maxPathLength> lastRom{};
	std::array<char, 16> playbackProfile{};
	std::array<char, 8> display{};
	std::array<char, 20> palette{};
	std::array<char, 16> speedMode{};
	std::array<char, 16> warmupMode{};
	std::array<char, 16> romStorage{};
	std::uint32_t renderEvery{4};
	bool syncRealtime{true};
	std::uint32_t inputEvery{2};
	std::uint32_t exitHoldMs{1000};
	bool audioEnabled{false};
	bool romFlashReuse{false};
	bool fastTiming{true};
	bool fastHalt{true};
	std::uint32_t frames{16384};
	std::uint32_t stepsPerFrame{512};
	std::uint32_t cyclesPerFrame{0};
	std::uint32_t maxWarmupCycles{8988672};
	std::uint32_t minTileData{256};
	bool noPpu{false};
	bool debugProfile{false};
	std::uint32_t profileEvery{120};
	bool faultTrace{false};

	[[nodiscard]] std::string_view romDirectoryPath() const;
	[[nodiscard]] std::string_view lastRomPath() const;
	[[nodiscard]] std::string_view playbackProfileName() const;
	[[nodiscard]] std::string_view displayName() const;
	[[nodiscard]] std::string_view paletteName() const;
	[[nodiscard]] std::string_view speedModeName() const;
	[[nodiscard]] std::string_view warmupModeName() const;
	[[nodiscard]] std::string_view romStorageName() const;
};

enum class GbEmuSettingsStatus : std::uint8_t {
	Loaded,
	CreatedDefault,
	StorageUnavailable,
	InvalidFile,
	ReadFailed,
	WriteFailed,
	SyncFailed,
};

struct GbEmuSettingsSaveResult {
	GbEmuSettingsStatus status{GbEmuSettingsStatus::WriteFailed};
	lexxos::vfs::WriteStatus writeStatus{lexxos::vfs::WriteStatus::Ok};
	lexxos::vfs::SyncStatus syncStatus{lexxos::vfs::SyncStatus::Ok};

	[[nodiscard]] bool ok() const {
		return status == GbEmuSettingsStatus::Loaded;
	}
};

struct GbEmuSettingsLoadResult {
	GbEmuSettings settings{};
	GbEmuSettingsStatus status{GbEmuSettingsStatus::StorageUnavailable};
	lexxos::vfs::OpenStatus openStatus{lexxos::vfs::OpenStatus::Ok};
	lexxos::vfs::ReadStatus readStatus{lexxos::vfs::ReadStatus::Ok};
	lexxos::vfs::WriteStatus writeStatus{lexxos::vfs::WriteStatus::Ok};
	lexxos::vfs::SyncStatus syncStatus{lexxos::vfs::SyncStatus::Ok};
};

[[nodiscard]] GbEmuSettings defaultGbEmuSettings();
[[nodiscard]] bool parseGbEmuSettings(std::string_view text, GbEmuSettings& settings);
[[nodiscard]] bool setGbEmuRomDirectory(GbEmuSettings& settings, std::string_view value);
[[nodiscard]] bool setGbEmuLastRom(GbEmuSettings& settings, std::string_view value);
[[nodiscard]] GbEmuSettingsLoadResult loadOrCreateGbEmuSettings();
[[nodiscard]] GbEmuSettingsSaveResult saveGbEmuSettings(GbEmuSettings const& settings);

} // namespace lexxos::apps::gameboy_programs::detail
