#include <lexxos/app/runtime.h>

#include <lexxos/app/console_mirror.h>
#include <lexxos/app/console_output.h>
#include <lexxos/app/runtime_tasks.h>
#include <lexxos/app/wifi_autoconnect.h>
#include <lexxos/apps/programs.h>
#include <lexxos/board_config.h>
#include <lexxos/composition/board_device_providers.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(SD_CARD)
	#include <lexxos/devices/sd_card.h>
#endif

#if FEATURE_ENABLED(SD_CARD) && BOARD_FAMILY_ENABLED(JC_ESP32P4_M3_DEV)
	#include <lexxos/apps/devices/touch_programs.h>
	#include <lexxos/apps/rtc/detail/rtc_settings_store.h>
	#include <lexxos/devices/rtc/software_rtc.h>
#endif

#if FEATURE_ENABLED(FATFS)
	#include <lexxos/fs/fat/fat_volume.h>
#endif

#if FEATURE_ENABLED(SD_CARD)
namespace sd_card = lexxos::devices::sd_card;
#endif

namespace {

#if FEATURE_ENABLED(SD_CARD) && BOARD_FAMILY_ENABLED(PICOCALC)
constexpr std::string_view picoCalcBacklightConfigPath = "/mnt/sd/.lexx/backlight.conf";
constexpr std::string_view legacyPicoCalcBacklightConfigPath = "/mnt/sd/lexxos/picocalc/backlight.conf";
#endif

lexxos::platform::PlatformServices const* g_schedulerPlatform{nullptr};

void writeConsole(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	lexxos::app::console_output::write(platform, text);
}

void writeConsoleLine(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	writeConsole(platform, text);
	writeConsole(platform, "\r\n");
}

void schedulerSleepMs(std::uint32_t milliseconds) {
	auto const* platform = g_schedulerPlatform;
	if (platform != nullptr && platform->clock.sleepMs != nullptr) {
		platform->clock.sleepMs(milliseconds);
		return;
	}

	lexxos::platform::sleepMs(milliseconds);
}

void schedulerYield() {
	schedulerSleepMs(0);
}

void schedulerWriteConsole(std::string_view text) {
	auto const* platform = g_schedulerPlatform;
	if (platform != nullptr) {
		lexxos::app::console_output::write(*platform, text);
		return;
	}

	lexxos::platform::writeConsole(text);
}

void bindRuntimeSchedulerBackend(lexxos::platform::PlatformServices const& platform) {
	g_schedulerPlatform = &platform;
	lexxos::kernel::bindSchedulerBackend({
	    .sleepMs = schedulerSleepMs,
	    .yield = schedulerYield,
	    .writeConsole = schedulerWriteConsole,
	});
}

#if FEATURE_ENABLED(SD_CARD) && BOARD_FAMILY_ENABLED(PICOCALC)
void restorePicoCalcBacklightIfConfigured(
    lexxos::os::OperatingSystem& operatingSystem,
    lexxos::platform::PlatformServices const& platform) {
	lexxos::vfs::Stat config{};
	bool const hasConfig =
	    operatingSystem.vfs().stat(picoCalcBacklightConfigPath, config)
	    && config.type == lexxos::vfs::NodeType::RegularFile;
	bool const hasLegacyConfig =
	    operatingSystem.vfs().stat(legacyPicoCalcBacklightConfigPath, config)
	    && config.type == lexxos::vfs::NodeType::RegularFile;
	if (!hasConfig && !hasLegacyConfig) {
		return;
	}

	auto context = operatingSystem.createAppContext({
	    .owner = lexxos::os::AppContextOwner::Kernel,
	    .workingDirectory = "/",
	});
	char const* argv[] = {"backlight", "restore"};
	auto const result = context.launch({
	    .name = "backlight",
	    .argc = 2,
	    .argv = argv,
	});
	if (!result.ok()) {
		writeConsoleLine(platform, "backlight auto-restore: failed");
	}
}
#endif

#if FEATURE_ENABLED(SD_CARD) && BOARD_FAMILY_ENABLED(JC_ESP32P4_M3_DEV)
void restoreRtcSettingsIfConfigured(lexxos::platform::PlatformServices const& platform) {
	auto const& board = platform.board.current != nullptr ? platform.board.current() : lexxos::platform::board();
	if (!lexxos::devices::rtc::software::configured(board)) {
		return;
	}

	lexxos::apps::rtc_programs::detail::RtcSettings settings{};
	auto const status = lexxos::apps::rtc_programs::detail::loadRtcSettings(
	    settings,
	    lexxos::devices::rtc::software::config(platform));
	if (status == lexxos::apps::rtc_programs::detail::RtcSettingsStatus::NotFound) {
		return;
	}
	if (status != lexxos::apps::rtc_programs::detail::RtcSettingsStatus::Ok) {
		writeConsole(platform, "rtc-config auto-restore: skipped: ");
		writeConsole(platform, lexxos::apps::rtc_programs::detail::toString(status));
		writeConsole(platform, "\r\n");
		return;
	}

	auto const config = lexxos::apps::rtc_programs::detail::softwareRtcConfigFromSettings(settings);
	if (!lexxos::devices::rtc::software::applyRuntimeConfigOverride(
	        platform,
	        config,
	        lexxos::apps::rtc_programs::detail::rtcSettingsServer(settings))) {
		writeConsoleLine(platform, "rtc-config auto-restore: failed");
		return;
	}
	writeConsole(platform, "rtc-config: restored path=");
	writeConsole(platform, lexxos::apps::rtc_programs::detail::rtcSettingsPath);
	writeConsole(platform, "\r\n");
}

void restoreJcTouchCalibrationIfConfigured(
    lexxos::os::OperatingSystem& operatingSystem,
    lexxos::platform::PlatformServices const& platform) {
	lexxos::vfs::Stat config{};
	if (!operatingSystem.vfs().stat(lexxos::apps::device_programs::touchCalibrationConfigPath, config)
	    || config.type != lexxos::vfs::NodeType::RegularFile) {
		return;
	}

	auto context = operatingSystem.createAppContext({
	    .owner = lexxos::os::AppContextOwner::Kernel,
	    .workingDirectory = "/",
	});
	char const* argv[] = {"touch-calibrate", "restore"};
	auto const result = context.launch({
	    .name = "touch-calibrate",
	    .argc = 2,
	    .argv = argv,
	});
	if (!result.ok()) {
		writeConsoleLine(platform, "touch calibration auto-restore: failed");
	}
}

void restoreSdConfigsAfterMount(lexxos::os::OperatingSystem& operatingSystem, std::string_view reason) {
	(void) reason;
	restoreRtcSettingsIfConfigured(operatingSystem.platform());
	restoreJcTouchCalibrationIfConfigured(operatingSystem, operatingSystem.platform());
	(void) lexxos::app::triggerWifiAutoconnect(operatingSystem, false, false);
}
#endif

} // namespace

int lexxos::app::Runtime::run() {
	initializeSystem();

#if FEATURE_ENABLED(DEBUG_DISPLAY)
	bool const debugDisplayReady = initializeDebugDisplay();
#endif

	console_mirror::initialize(_operatingSystem);

#if FEATURE_ENABLED(SD_CARD)
	#if BOARD_FAMILY_ENABLED(JC_ESP32P4_M3_DEV)
	_sdAutomount.setPostMountHandler(restoreSdConfigsAfterMount);
	#endif
	initializeStorage();
#endif

#if FEATURE_ENABLED(DEBUG_DISPLAY)
	if (debugDisplayReady) {
		_debugDisplay.refresh();
	}
#endif

#if FEATURE_ENABLED(SD_CARD)
	if (_operatingSystem.capabilities().has(lexxos::runtime::Capability::StorageRemovable)
	    && _operatingSystem.capabilities().has(lexxos::runtime::Capability::StorageBlock)) {
		_systemEvents.bindSdAutomount(_sdAutomount);
	}
#endif

	_systemEvents.setCapabilities(_operatingSystem.capabilities());
	_systemEvents.initialize();

	_taskContext = {
	    .operatingSystem = &_operatingSystem,
	    .shell = &_shell,
	    .systemEvents = &_systemEvents,
#if FEATURE_ENABLED(DEBUG_DISPLAY)
	    .debugDisplay = &_debugDisplay,
	    .debugDisplayReady = debugDisplayReady,
#endif
	};

	if (!startRuntimeTasks(_taskContext)) {
		return 1;
	}

	lexxos::kernel::startScheduler();
	return 0;
}

void lexxos::app::Runtime::initializeSystem() {
	bindRuntimeSchedulerBackend(_platformServices);
	_platformServices.lifecycle.initialize();

	lexxos::kernel::resetShutdownRequest();
	_operatingSystem.vfs().initialize(_platformServices);

#if FEATURE_ENABLED(FATFS)
	lexxos::fs::fat::registerVfsFactory(_operatingSystem.vfs());
#endif

	_operatingSystem.initialize();
	lexxos::composition::registerBoardDevices(_operatingSystem.devices(), _platformServices);
	_operatingSystem.refreshCapabilities();

	_shell.bindOperatingSystem(_operatingSystem);
	registerShellProgram(_operatingSystem.programs());
	lexxos::apps::registerBuiltinPrograms(_operatingSystem.programs());
}

#if FEATURE_ENABLED(DEBUG_DISPLAY)
bool lexxos::app::Runtime::initializeDebugDisplay() {
	bool const debugDisplayReady = _debugDisplay.initialize();
	if (debugDisplayReady) {
		_debugDisplay.drawStatic();
		_debugDisplay.showBooting();
	}
	#if defined(LEXXOS_PLATFORM_PICO) || defined(LEXXOS_PLATFORM_STM32F4)
	else {
		writeConsoleLine(_platformServices, "debug display is not available");
	}
	#endif
	return debugDisplayReady;
}
#endif

#if FEATURE_ENABLED(SD_CARD)
void lexxos::app::Runtime::initializeStorage() {
	auto const capabilities = _operatingSystem.capabilities();
	if (!capabilities.has(lexxos::runtime::Capability::StorageBlock)) {
		return;
	}

	bool sdReady = false;
	bool const deferProbeUntilScheduler =
	#if defined(LEXXOS_KERNEL_FREERTOS)
	    capabilities.has(lexxos::runtime::Capability::StorageRemovable);
	#else
	    false;
	#endif
	if (!deferProbeUntilScheduler) {
		sdReady = sd_card::initialize(_platformServices);
	}

	if (auto* sdBlockDevice = _operatingSystem.devices().blockDevice("sd0"); sdBlockDevice != nullptr) {
		_operatingSystem.vfs().registerBlockDevice("sd0", sdBlockDevice);
	}

	#if defined(LEXXOS_PLATFORM_PICO) || defined(LEXXOS_PLATFORM_STM32F4) || defined(LEXXOS_PLATFORM_ESPIDF)
	writeConsole(_platformServices, "sd card: ");
	writeConsole(_platformServices, deferProbeUntilScheduler ? "deferred" : (sdReady ? "ready" : lexxos::devices::toString(sd_card::state())));
	writeConsole(_platformServices, "\r\n");
	#endif

	if (!deferProbeUntilScheduler
	    && capabilities.has(lexxos::runtime::Capability::StorageRemovable)
	    && (sdReady || (sd_card::hasCardDetect() && sd_card::isCardInserted()))) {
		bool const mounted = _sdAutomount.mount("boot");

	#if BOARD_FAMILY_ENABLED(PICOCALC)
		if (mounted && capabilities.has(lexxos::runtime::Capability::PicoCalcSouthbridge)) {
			restorePicoCalcBacklightIfConfigured(_operatingSystem, _platformServices);
		}
	#else
		static_cast<void>(mounted);
	#endif
	}
}
#endif
