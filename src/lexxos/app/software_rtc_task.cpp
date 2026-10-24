#include <lexxos/app/software_rtc_task.h>

#include <cstdint>
#include <string_view>

#include <lexxos/app/runtime_tasks.h>
#include <lexxos/devices/rtc/software_rtc.h>
#include <lexxos/feature.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <lexxos/apps/network/detail/wifi_program_support.h>
	#include <lexxos/apps/network/network_time.h>
	#include <lexxos/devices/espidf/espidf_ethernet.h>
	#define LEXXOS_SOFTWARE_RTC_NETWORK_SYNC_AVAILABLE 1
#elif FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	#include <lexxos/apps/network/detail/wifi_program_support.h>
	#include <lexxos/apps/network/network_time.h>
	#define LEXXOS_SOFTWARE_RTC_NETWORK_SYNC_AVAILABLE 1
#else
	#define LEXXOS_SOFTWARE_RTC_NETWORK_SYNC_AVAILABLE 0
#endif

namespace {

lexxos::platform::PlatformServices const& platformServices(lexxos::app::RuntimeTaskContext const& context) {
	if (context.operatingSystem != nullptr) {
		return context.operatingSystem->platform();
	}
	return lexxos::platform::services();
}

std::uint32_t platformUptimeMs(lexxos::platform::PlatformServices const& platform) {
	auto const& clock = platform.clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

#if LEXXOS_SOFTWARE_RTC_NETWORK_SYNC_AVAILABLE
lexxos::platform::BoardDescriptor const& activeBoard(lexxos::app::RuntimeTaskContext const& context) {
	auto const& platform = platformServices(context);
	auto const& board = platform.board;
	return board.current != nullptr ? board.current() : lexxos::platform::board();
}
#endif

bool softwareRtcNetworkReady(lexxos::app::RuntimeTaskContext& context) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	auto const& board = activeBoard(context);
	if (board.ethernet.enabled) {
		auto const status = lexxos::devices::espidf_ethernet::status();
		if (status.supported && status.initialized && status.started && status.linkUp && status.gotIp) {
			return true;
		}
	}
	if (board.wireless.wifi) {
		auto const wifi = lexxos::apps::network_programs::detail::readWifiRuntimeStatus(board);
		if (wifi.supported && wifi.initialized && wifi.connected && wifi.gotIp) {
			return true;
		}
	}
	return false;
#elif FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	auto const wifi = lexxos::apps::network_programs::detail::readWifiRuntimeStatus(activeBoard(context));
	return wifi.supported && wifi.initialized && wifi.connected && wifi.gotIp;
#else
	(void) context;
	return false;
#endif
}

std::uint32_t positiveOrFallback(std::uint32_t value, std::uint32_t fallback) {
	return value == 0U ? fallback : value;
}

bool softwareRtcNetworkSyncDue(
    lexxos::platform::PlatformServices const& platform,
    lexxos::platform::SoftwareRtcConfig const& config,
    std::uint32_t nowMs) {
	auto const status = lexxos::devices::rtc::software::status(platform);
	if (!status.valid || !status.lastNetworkAttemptValid) {
		return true;
	}

	if (status.lastNetworkAttemptOk) {
		auto const interval = positiveOrFallback(config.networkSyncIntervalMs, 60U * 60U * 1000U);
		return static_cast<std::int32_t>(nowMs - (status.lastNetworkSyncUptimeMs + interval)) >= 0;
	}

	auto const retryInterval = positiveOrFallback(config.networkSyncRetryIntervalMs, 60U * 1000U);
	return static_cast<std::int32_t>(nowMs - (status.lastNetworkAttemptUptimeMs + retryInterval)) >= 0;
}

void runSoftwareRtcNetworkSync(
    lexxos::app::RuntimeTaskContext& runtimeContext,
    lexxos::platform::SoftwareRtcConfig const& config) {
#if LEXXOS_SOFTWARE_RTC_NETWORK_SYNC_AVAILABLE
	if (runtimeContext.operatingSystem == nullptr || !softwareRtcNetworkReady(runtimeContext)) {
		return;
	}

	auto& quietOutput = lexxos::io::nullOutputStream();
	auto context = runtimeContext.operatingSystem->createAppContext({
	    .owner = lexxos::os::AppContextOwner::Kernel,
	    .workingDirectory = "/",
	    .standardOutput = &quietOutput,
	    .standardError = &quietOutput,
	});

	lexxos::devices::rtc::DateTime networkTime{};
	std::uint32_t unixSeconds = 0;
	auto const server = config.networkSyncServer != nullptr ? std::string_view(config.networkSyncServer) : std::string_view("pool.ntp.org");
	auto const timeoutMs = positiveOrFallback(config.networkSyncTimeoutMs, 8000);
	bool const queried = lexxos::apps::network_programs::queryNetworkTime(context,
	    server,
	    timeoutMs,
	    networkTime,
	    unixSeconds);
	auto const adjustedUnixSeconds =
	    static_cast<std::int64_t>(unixSeconds) + static_cast<std::int64_t>(config.timezoneOffsetSeconds);
	bool const updated = queried
	    && adjustedUnixSeconds >= 0
	    && adjustedUnixSeconds <= 0xFFFFFFFFLL
	    && lexxos::devices::rtc::software::setUnixSeconds(
	        platformServices(runtimeContext),
	        static_cast<std::uint32_t>(adjustedUnixSeconds),
	        lexxos::devices::rtc::software::SetSource::Network);
	lexxos::devices::rtc::software::recordNetworkAttempt(platformServices(runtimeContext), updated);
#else
	(void) runtimeContext;
	(void) config;
#endif
}

void softwareRtcTask(void* userData) {
	auto& runtimeContext = *static_cast<lexxos::app::RuntimeTaskContext*>(userData);
	auto const& platform = platformServices(runtimeContext);
	if (!lexxos::devices::rtc::software::configured(platform)) {
		return;
	}

	auto config = lexxos::devices::rtc::software::config(platform);
	if (config.startupDelayMs != 0U) {
		lexxos::kernel::sleepMs(config.startupDelayMs);
	}

	while (!lexxos::kernel::isShutdownRequested()) {
		config = lexxos::devices::rtc::software::config(platform);
		if (!config.enabled) {
			return;
		}

		if (config.autoNetworkSync
		    && softwareRtcNetworkSyncDue(platform, config, platformUptimeMs(platform))) {
			runSoftwareRtcNetworkSync(runtimeContext, config);
		}
		lexxos::kernel::sleepMs(1000);
	}
}

} // namespace

bool lexxos::app::startSoftwareRtcTask(RuntimeTaskContext& context) {
	if (!lexxos::devices::rtc::software::configured(platformServices(context))) {
		return true;
	}
	return lexxos::kernel::createTask(softwareRtcTask, &context, {
	                                                                 .name = "software-rtc",
	                                                                 .stackSizeBytes = 6144,
	                                                                 .priority = 1,
	                                                                 .executionClass = lexxos::kernel::TaskExecutionClass::Background,
	                                                             });
}
