#include <lexxos/app/sd_automount_service.h>

#include <array>
#include <cstdint>

#include <lexxos/app/console_output.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/feature.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(FATFS)
	#include <lexxos/devices/storage/sd_spi_backend.h>
	#include <lexxos/fs/fat/fat_volume.h>
	#define LEXXOS_SD_AUTOMOUNT_HAS_FAT 1
#else
	#define LEXXOS_SD_AUTOMOUNT_HAS_FAT 0
#endif

namespace sd_card = lexxos::devices::sd_card;

#if LEXXOS_SD_AUTOMOUNT_HAS_FAT
namespace fat = lexxos::fs::fat;
#endif

namespace {

constexpr std::uint8_t sdAutoMountProbeAttempts = 4;
constexpr std::uint8_t sdAutoMountProbeRounds = 4;
constexpr std::uint32_t sdAutoMountBootSettleMs = 1000;
constexpr std::uint32_t sdAutoMountProbeRetryDelayMs = 500;

#if LEXXOS_SD_AUTOMOUNT_HAS_FAT
std::array<std::uint8_t, lexxos::devices::SdSpiBackend::blockSize> g_sdAutoMountSector{};
#endif

void writeConsole(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	lexxos::app::console_output::write(platform, text);
}

void writeConsoleLine(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	writeConsole(platform, text);
	writeConsole(platform, "\r\n");
}

void sleepMs(lexxos::platform::PlatformServices const& platform, std::uint32_t milliseconds) {
	if (platform.clock.sleepMs != nullptr) {
		platform.clock.sleepMs(milliseconds);
	}
}

#if LEXXOS_SD_AUTOMOUNT_HAS_FAT
bool readSdBlock0ForAutoMount(lexxos::devices::DeviceManager& devices) {
	auto* device = devices.blockDevice("sd0");
	if (device == nullptr || !device->isReady()) {
		return false;
	}

	return device->readBlocks(0, g_sdAutoMountSector.data(), 1);
}

bool ensureSdReadyForAutoMount(lexxos::platform::PlatformServices const& platform, lexxos::devices::DeviceManager& devices) {
	if (sd_card::hasCardDetect() && !sd_card::isCardInserted()) {
		return false;
	}

	for (std::uint8_t round = 0; round < sdAutoMountProbeRounds; ++round) {
		if (sd_card::probe(platform, sdAutoMountProbeAttempts) && readSdBlock0ForAutoMount(devices)) {
			return true;
		}
		if (round + 1U < sdAutoMountProbeRounds) {
			writeConsoleLine(platform, "sd automount: probe failed, retrying");
			sleepMs(platform, sdAutoMountProbeRetryDelayMs);
		}
	}
	return false;
}
#endif

} // namespace

lexxos::app::SdAutomountService::SdAutomountService(lexxos::os::OperatingSystem& operatingSystem)
    : _operatingSystem(operatingSystem) {
}

void lexxos::app::SdAutomountService::setPostMountHandler(SdPostMountHandler handler) {
	_postMountHandler = handler;
}

bool lexxos::app::SdAutomountService::mount(std::string_view reason) {
#if !LEXXOS_SD_AUTOMOUNT_HAS_FAT
	(void) reason;
	return false;
#else

	auto const& platform = _operatingSystem.platform();

	if (sd_card::hasCardDetect() && !sd_card::isCardInserted()) {
		return false;
	}

	if (fat::isMounted(mountPath)) {
		return true;
	}
	if (_operatingSystem.vfs().isMountTarget(mountPath)) {
		writeConsole(platform, "sd automount: stale mount at ");
		writeConsole(platform, mountPath);
		writeConsoleLine(platform, ", force unmounting");
		fat::forceUnmount(_operatingSystem.vfs(), mountPath);
	}

	auto& devices = _operatingSystem.devices();
	if (devices.blockDevice("sd0") == nullptr) {
		writeConsoleLine(platform, "sd automount: sd0 is not registered in DeviceManager");
		return false;
	}
	auto sdLock = devices.lockBlockDevice("sd0", lexxos::devices::DeviceResourcePriority::System, "sd-automount");
	if (!sdLock) {
		writeConsoleLine(platform, "sd automount: sd0 is busy");
		return false;
	}

	if (reason == "boot") {
		writeConsoleLine(platform, "sd automount: probing (boot)");
		sleepMs(platform, sdAutoMountBootSettleMs);
	} else if (reason == "inserted") {
		writeConsoleLine(platform, "sd automount: probing (inserted)");
	} else if (reason.empty()) {
		writeConsoleLine(platform, "sd automount: probing");
	} else {
		writeConsole(platform, "sd automount: probing (");
		writeConsole(platform, reason);
		writeConsoleLine(platform, ")");
	}

	if (!ensureSdReadyForAutoMount(platform, devices)) {
		writeConsole(platform, "sd automount: probe failed, state=");
		writeConsole(platform, lexxos::devices::toString(sd_card::state()));
		writeConsoleLine(platform, "");
		return false;
	}

	auto const status = fat::mount(_operatingSystem.vfs(), blockDevicePath, mountPath);
	if (status == lexxos::fs::fat::FatMountStatus::Ok || status == lexxos::fs::fat::FatMountStatus::AlreadyMounted) {
		writeConsole(platform, "sd automount: mounted ");
		writeConsole(platform, blockDevicePath);
		writeConsole(platform, " on ");
		writeConsole(platform, mountPath);
		writeConsoleLine(platform, " type fat");
		if (_postMountHandler != nullptr) {
			_postMountHandler(_operatingSystem, reason);
		}
		return true;
	}

	writeConsole(platform, "sd automount: mount failed: ");
	writeConsole(platform, lexxos::fs::fat::toString(status));
	writeConsoleLine(platform, "");
	return false;

#endif
}

void lexxos::app::SdAutomountService::unmount(std::string_view reason) {
#if !LEXXOS_SD_AUTOMOUNT_HAS_FAT
	(void) reason;
#else
	if (!fat::isMounted(mountPath) && !_operatingSystem.vfs().isMountTarget(mountPath)) {
		return;
	}

	auto const& platform = _operatingSystem.platform();
	bool const force = reason == "removed";
	auto const status = force ? fat::forceUnmount(_operatingSystem.vfs(), mountPath) : fat::unmount(_operatingSystem.vfs(), mountPath);
	if (force) {
		writeConsole(platform, "sd automount: force-unmounted ");
		writeConsole(platform, mountPath);
		writeConsole(platform, " (card removed");
		if (status != lexxos::vfs::UnmountStatus::Ok) {
			writeConsole(platform, ", ");
			writeConsole(platform, lexxos::vfs::toString(status));
		}
		writeConsoleLine(platform, ")");
		return;
	}

	writeConsole(platform, "sd automount: unmounted ");
	writeConsole(platform, mountPath);
	if (!reason.empty()) {
		writeConsole(platform, " (");
		writeConsole(platform, reason);
		writeConsole(platform, ")");
	}
	if (status != lexxos::vfs::UnmountStatus::Ok) {
		writeConsole(platform, ": ");
		writeConsole(platform, lexxos::vfs::toString(status));
	}
	writeConsoleLine(platform, "");
#endif
}

bool lexxos::app::SdAutomountService::isMounted() const {
#if !LEXXOS_SD_AUTOMOUNT_HAS_FAT
	return false;
#else
	return fat::isMounted(mountPath);
#endif
}
