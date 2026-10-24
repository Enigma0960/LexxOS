#include <lexxos/app/wifi_autoconnect.h>

#include <atomic>
#include <cstdint>
#include <string_view>

#include <lexxos/app/console_output.h>
#include <lexxos/app/runtime_tasks.h>
#include <lexxos/feature.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform.h>
#include <lexxos/program/program_supervisor.h>
#include <lexxos/runtime/capabilities.h>

#if defined(LEXXOS_PICOCALC_WIFI_AUTOCONNECT) && !defined(LEXXOS_WIFI_AUTOCONNECT)
	#define LEXXOS_WIFI_AUTOCONNECT LEXXOS_PICOCALC_WIFI_AUTOCONNECT
#endif

#if !defined(LEXXOS_WIFI_AUTOCONNECT)
	#if FEATURE_ENABLED(SD_CARD)            \
	    && (defined(LEXXOS_PLATFORM_ESPIDF) \
	        || (FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)))
		#define LEXXOS_WIFI_AUTOCONNECT 1
	#else
		#define LEXXOS_WIFI_AUTOCONNECT 0
	#endif

	#include <lexxos/apps/network/detail/wifi_credentials_store.h>
	#include <lexxos/apps/network/detail/wifi_program_support.h>
#endif

namespace {

#if LEXXOS_WIFI_AUTOCONNECT
std::atomic_bool g_wifiAutoconnectStarted{false};

// Keep Wi-Fi auto-connect out of the console boot path. The shell banner should
// become visible even if reading credentials from the SD card or starting the
// board network backend takes a long time.
constexpr std::uint32_t wifiAutoconnectDelayMs = 1500;
constexpr std::uint32_t wifiAutoconnectCredentialsRetryDelayMs = 500;
constexpr std::uint8_t wifiAutoconnectCredentialsAttempts = 12;
constexpr std::uint32_t wifiAutoconnectTaskStackSizeBytes = 4096;

lexxos::platform::BoardDescriptor const& activeBoard(lexxos::os::OperatingSystem const& operatingSystem) {
	auto const& platform = operatingSystem.platform();
	auto const& board = platform.board;
	return board.current != nullptr ? board.current() : lexxos::platform::board();
}

void writeConsole(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	lexxos::app::console_output::write(platform, text);
}

void formatUnsignedDecimal(std::uint32_t value, char (&buffer)[11]) {
	std::size_t index = sizeof(buffer) - 1U;
	buffer[index] = '\0';
	do {
		buffer[--index] = static_cast<char>('0' + (value % 10U));
		value /= 10U;
	} while (value != 0 && index > 0);

	std::size_t output = 0;
	while (index < sizeof(buffer)) {
		buffer[output++] = buffer[index++];
	}
}

lexxos::apps::network_programs::detail::WifiCredentialsStatus loadWifiCredentialsForAutoconnect(
    lexxos::apps::network_programs::detail::WifiCredentials& credentials) {
	using lexxos::apps::network_programs::detail::WifiCredentialsStatus;

	WifiCredentialsStatus lastStatus = WifiCredentialsStatus::NotFound;
	for (std::uint8_t attempt = 0; attempt < wifiAutoconnectCredentialsAttempts; ++attempt) {
		lastStatus = lexxos::apps::network_programs::detail::loadWifiCredentials(credentials);
		if (lastStatus != WifiCredentialsStatus::NotFound && lastStatus != WifiCredentialsStatus::IoError) {
			return lastStatus;
		}
		if (attempt + 1U < wifiAutoconnectCredentialsAttempts) {
			lexxos::kernel::sleepMs(wifiAutoconnectCredentialsRetryDelayMs);
		}
	}
	return lastStatus;
}

void wifiAutoconnectTask(void* userData) {
	auto& runtimeContext = *static_cast<lexxos::app::RuntimeTaskContext*>(userData);
	if (runtimeContext.operatingSystem != nullptr) {
		(void) lexxos::app::triggerWifiAutoconnect(*runtimeContext.operatingSystem, true, true);
	}
}
#endif

} // namespace

bool lexxos::app::triggerWifiAutoconnect(
    lexxos::os::OperatingSystem& operatingSystem,
    bool initialDelay,
    bool retryCredentials) {
#if LEXXOS_WIFI_AUTOCONNECT
	auto const& board = activeBoard(operatingSystem);
	auto const& platform = operatingSystem.platform();
	if (!board.wireless.autoConnect) {
		return false;
	}
	if (!operatingSystem.capabilities().has(lexxos::runtime::Capability::WirelessWifi)
	    || !operatingSystem.capabilities().has(lexxos::runtime::Capability::StorageBlock)) {
		return false;
	}

	if (initialDelay) {
		lexxos::kernel::sleepMs(wifiAutoconnectDelayMs);
	}

	lexxos::apps::network_programs::detail::WifiCredentials credentials{};
	auto const status = retryCredentials
	    ? loadWifiCredentialsForAutoconnect(credentials)
	    : lexxos::apps::network_programs::detail::loadWifiCredentials(credentials);
	if (status == lexxos::apps::network_programs::detail::WifiCredentialsStatus::NotFound
	    || status == lexxos::apps::network_programs::detail::WifiCredentialsStatus::Disabled) {
		return false;
	}
	if (status != lexxos::apps::network_programs::detail::WifiCredentialsStatus::Ok) {
		writeConsole(platform, "wifi-autoconnect: skipped: ");
		writeConsole(platform, lexxos::apps::network_programs::detail::toString(status));
		writeConsole(platform, "\r\n");
		return false;
	}

	auto const runtimeStatus = lexxos::apps::network_programs::detail::readWifiRuntimeStatus(board);
	if (runtimeStatus.supported && runtimeStatus.connected && runtimeStatus.gotIp) {
		return false;
	}

	bool expected = false;
	if (!g_wifiAutoconnectStarted.compare_exchange_strong(expected, true)) {
		return false;
	}

	auto const ssid = lexxos::apps::network_programs::detail::wifiCredentialsSsid(credentials);
	auto const auth = lexxos::apps::network_programs::detail::wifiCredentialsAuth(credentials);
	auto const password = lexxos::apps::network_programs::detail::wifiCredentialsPassword(credentials);

	writeConsole(platform, "wifi-autoconnect: connecting to ");
	writeConsole(platform, ssid);
	writeConsole(platform, "\r\n");

	auto context = operatingSystem.createAppContext({
	    .owner = lexxos::os::AppContextOwner::Kernel,
	    .workingDirectory = "/",
	});

	char timeoutText[11]{};
	formatUnsignedDecimal(credentials.timeoutMs, timeoutText);
	char const* argv[] = {"wifi-connect", "--auth", credentials.auth.data(), "--timeout", timeoutText, credentials.ssid.data(), credentials.password.data()};
	int const argc = (auth == "open" && password.empty()) ? 6 : 7;

	auto& quietOutput = lexxos::io::nullOutputStream();
	auto const result = lexxos::program::supervisor().startBackground(context, {
	                                                                               .name = "wifi-connect",
	                                                                               .argc = argc,
	                                                                               .argv = argv,
	                                                                               .standardOutput = &quietOutput,
	                                                                               .standardError = &quietOutput,
	                                                                           });
	if (!result.started()) {
		g_wifiAutoconnectStarted.store(false);
		writeConsole(platform, "wifi-autoconnect: start failed: ");
		writeConsole(platform, lexxos::program::toString(result.status));
		writeConsole(platform, "\r\n");
		return false;
	}
	return true;
#else
	(void) operatingSystem;
	(void) initialDelay;
	(void) retryCredentials;
	return false;
#endif
}

void lexxos::app::startWifiAutoconnectTask(RuntimeTaskContext& context) {
#if LEXXOS_WIFI_AUTOCONNECT
	if (context.operatingSystem != nullptr
	    && context.operatingSystem->capabilities().has(lexxos::runtime::Capability::WirelessWifi)
	    && context.operatingSystem->capabilities().has(lexxos::runtime::Capability::StorageBlock)
	    && activeBoard(*context.operatingSystem).wireless.autoConnect
	    && !lexxos::kernel::createTask(wifiAutoconnectTask, &context, {
	                                                                      .name = "wifi-autoconnect",
	                                                                      .stackSizeBytes = wifiAutoconnectTaskStackSizeBytes,
	                                                                      .priority = 1,
	                                                                      .executionClass = lexxos::kernel::TaskExecutionClass::Background,
	                                                                  })) {
		writeConsole(context.operatingSystem->platform(), "failed to create wifi autoconnect task\r\n");
	}
#else
	(void) context;
#endif
}
