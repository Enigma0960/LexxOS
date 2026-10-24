#include <lexxos/apps/network/detail/wifi_program_support.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>

#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
	#include <lexxos/devices/espidf/espidf_wifi.h>
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	#include <lexxos/apps/network/detail/pico_network_support.h>
#endif

namespace lexxos::apps::network_programs::detail {
namespace {

#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
	constexpr std::uint32_t wifiConnectDefaultTimeoutMs = 30000;
	constexpr std::uint32_t wifiScanDefaultTimeoutMs = 10000;
	constexpr std::uint32_t wifiMaxTimeoutMs = 120000;
	constexpr std::uint32_t wifiDefaultLimit = 24;
	constexpr std::uint32_t wifiMaxResults = 32;

	bool parseOptionalUnsigned(
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view name,
	    std::uint32_t& value) {
		auto const text = arguments.value(name);
		if (text.empty()) {
			return true;
		}
		return lexxos::apps::parseUnsigned32(text, value);
	}

	void writeMac(lexxos::os::AppContext& context, std::uint8_t const mac[6]) {
		for (std::size_t index = 0; index < 6; ++index) {
			if (index != 0) {
				context.write(":");
			}
			writeHexByte(context, mac[index]);
		}
	}

	lexxos::devices::espidf_wifi::AuthMode parseAuthForEspIdf(
	    lexxos::os::AppContext& context,
	    std::string_view text,
	    bool& ok) {
		lexxos::devices::espidf_wifi::AuthMode auth{};
		ok = lexxos::devices::espidf_wifi::parseAuthMode(text, auth);
		if (!ok) {
			context.writeErrorLine("wifi-connect: invalid --auth, expected auto, open, wpa, wpa2 or mixed");
		}
		return auth;
	}

	void writeEspIdfWifiStatus(lexxos::os::AppContext& context, lexxos::devices::espidf_wifi::Status const& status) {
		context.writeLine("interface: wifi");
		context.write("supported: ");
		context.writeLine(status.supported ? "yes" : "no");
		context.write("initialized: ");
		context.writeLine(status.initialized ? "yes" : "no");
		context.write("sta-mode: ");
		context.writeLine(status.staModeEnabled ? "enabled" : "disabled");
		context.write("started: ");
		context.writeLine(status.started ? "yes" : "no");
		context.write("link: ");
		context.writeLine(status.connected ? "up" : "down");
		context.write("mac: ");
		if (status.macValid) {
			writeMac(context, status.mac);
			context.writeLine("");
		} else {
			context.writeLine("-");
		}
		context.write("hostname: ");
		context.writeLine(status.hostname[0] != '\0' ? status.hostname : "-");
		context.write("ssid: ");
		context.writeLine(status.ssid[0] != '\0' ? status.ssid : "-");
		context.write("rssi: ");
		if (status.rssiValid) {
			if (status.rssi < 0) {
				context.write("-");
				writeUnsigned(context, static_cast<std::uint32_t>(-status.rssi));
			} else {
				writeUnsigned(context, static_cast<std::uint32_t>(status.rssi));
			}
			context.write(" dBm level=");
			writeUnsigned(context, status.signalLevel);
			context.writeLine("");
		} else {
			context.writeLine("-");
		}
		context.write("ip: ");
		context.writeLine(status.gotIp ? status.ip : "-");
		context.write("netmask: ");
		context.writeLine(status.gotIp ? status.netmask : "-");
		context.write("gateway: ");
		context.writeLine(status.gotIp ? status.gateway : "-");
		context.write("dns0: ");
		context.writeLine(status.dns0[0] != '\0' ? status.dns0 : "-");
		context.write("dns1: ");
		context.writeLine(status.dns1[0] != '\0' ? status.dns1 : "-");
		context.write("last-error: ");
		context.writeLine(status.lastErrorName[0] != '\0' ? status.lastErrorName : "-");
		context.write("last-disconnect-reason: ");
		writeUnsigned(context, status.lastDisconnectReason);
		context.writeLine("");
	}

	void writeEspIdfScanRecord(
	    lexxos::os::AppContext& context,
	    lexxos::devices::espidf_wifi::ScanRecord const& record,
	    bool showBssid) {
		context.write("ssid=\"");
		context.write(record.ssid[0] != '\0' ? record.ssid : "<hidden>");
		context.write("\" rssi=");
		if (record.rssi < 0) {
			context.write("-");
			writeUnsigned(context, static_cast<std::uint32_t>(-record.rssi));
		} else {
			writeUnsigned(context, static_cast<std::uint32_t>(record.rssi));
		}
		context.write(" channel=");
		writeUnsigned(context, record.channel);
		context.write(" auth=");
		context.write(record.auth);
		if (showBssid && record.bssidValid) {
			context.write(" bssid=");
			writeMac(context, record.bssid);
		}
		context.writeLine("");
	}
#endif

} // namespace

WifiRuntimeStatus readWifiRuntimeStatus(lexxos::platform::BoardDescriptor const& board) {
#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
	(void) board;
	auto const status = lexxos::devices::espidf_wifi::status();
	return {
	    .supported = status.supported,
	    .initialized = status.initialized,
	    .staModeEnabled = status.staModeEnabled,
	    .started = status.started,
	    .connected = status.connected,
	    .gotIp = status.gotIp,
	    .rssiValid = status.rssiValid,
	    .rssi = status.rssi,
	    .signalLevel = status.signalLevel,
	};
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	(void) board;
	auto const status = lexxos::apps::network_programs::pico_detail::readWifiRuntimeStatusOnPico();
	return {
	    .supported = status.supported,
	    .initialized = status.initialized,
	    .staModeEnabled = status.staModeEnabled,
	    .started = status.initialized,
	    .connected = status.connected,
	    .gotIp = status.connected,
	    .rssiValid = status.rssiValid,
	    .rssi = status.rssi,
	    .signalLevel = status.signalLevel,
	};
#else
	(void) board;
	return {};
#endif
}

int runWifiConnect(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board) {
	if (!board.wireless.wifi) {
		context.writeLine("wifi-connect: Wi-Fi is not declared by this board profile");
		return 1;
	}

#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
	std::uint32_t timeoutMs = wifiConnectDefaultTimeoutMs;
	if (!parseOptionalUnsigned(arguments, "timeout", timeoutMs) || timeoutMs == 0U || timeoutMs > wifiMaxTimeoutMs) {
		context.writeErrorLine("wifi-connect: invalid --timeout");
		return 2;
	}
	auto const ssid = arguments.positional("ssid");
	auto const password = arguments.positionalOr("password", "");
	if (ssid.empty()) {
		context.writeErrorLine("wifi-connect: missing SSID");
		return 2;
	}
	if (ssid.size() > 32U) {
		context.writeErrorLine("wifi-connect: SSID is too long");
		return 2;
	}
	if (password.size() > 64U) {
		context.writeErrorLine("wifi-connect: password is too long");
		return 2;
	}
	bool authOk = false;
	auto const auth = parseAuthForEspIdf(context, arguments.valueOr("auth", "auto"), authOk);
	if (!authOk) {
		return 2;
	}
	if (auth != lexxos::devices::espidf_wifi::AuthMode::Open && password.size() < 8U) {
		context.writeErrorLine("wifi-connect: password must contain at least 8 characters for WPA/WPA2");
		return 2;
	}

	context.write("wifi-connect: connecting to ");
	context.write(ssid);
	context.write(" auth=");
	context.write(lexxos::devices::espidf_wifi::toString(auth));
	context.writeLine("...");
	if (!lexxos::devices::espidf_wifi::connect(board, ssid, password, auth, timeoutMs)) {
		auto const status = lexxos::devices::espidf_wifi::status();
		context.writeError("wifi-connect: failed: ");
		context.writeErrorLine(status.lastErrorName[0] != '\0' ? status.lastErrorName : "unknown");
		if (status.lastDisconnectReason != 0U) {
			context.writeError("wifi-connect: disconnect reason ");
			writeUnsigned(context, status.lastDisconnectReason);
			context.writeErrorLine("");
		}
		return 1;
	}
	context.writeLine("wifi-connect: connected");
	return 0;
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	return lexxos::apps::network_programs::pico_detail::runWifiConnectOnPico(context, arguments, board);
#else
	(void) context;
	(void) arguments;
	(void) board;
	context.writeLine("wifi-connect: Wi-Fi support is unavailable in this build");
	return 1;
#endif
}

int runWifiStatus(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board) {
	if (!board.wireless.wifi) {
		context.writeLine("wifi-status: Wi-Fi is not declared by this board profile");
		return 1;
	}

#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
	(void) arguments;
	(void) lexxos::devices::espidf_wifi::start(board);
	auto const status = lexxos::devices::espidf_wifi::status();
	writeEspIdfWifiStatus(context, status);
	return status.supported && status.initialized && status.started && status.connected && status.gotIp ? 0 : 1;
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	return lexxos::apps::network_programs::pico_detail::runWifiStatusOnPico(context, arguments, board);
#else
	(void) arguments;
	context.writeLine("wifi-status: Wi-Fi support is unavailable in this build");
	return 1;
#endif
}

int runWifiScan(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board) {
	if (!board.wireless.wifi) {
		context.writeLine("wifi-scan: Wi-Fi is not declared by this board profile");
		return 1;
	}
	if (!board.wireless.scan) {
		context.writeLine("wifi-scan: scan is disabled by this board profile");
		return 1;
	}

#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(LEXXOS_ESPIDF_ENABLE_WIFI_BACKEND)
	std::uint32_t timeoutMs = wifiScanDefaultTimeoutMs;
	std::uint32_t limit = wifiDefaultLimit;
	if (!parseOptionalUnsigned(arguments, "timeout", timeoutMs) || timeoutMs == 0U || timeoutMs > wifiMaxTimeoutMs) {
		context.writeErrorLine("wifi-scan: invalid --timeout");
		return 2;
	}
	if (!parseOptionalUnsigned(arguments, "limit", limit) || limit == 0U || limit > wifiMaxResults) {
		context.writeErrorLine("wifi-scan: invalid --limit");
		return 2;
	}

	std::array<lexxos::devices::espidf_wifi::ScanRecord, lexxos::devices::espidf_wifi::maxWifiScanResults> records{};
	std::size_t count = 0;
	context.writeLine("wifi-scan: scanning...");
	if (!lexxos::devices::espidf_wifi::scan(board, timeoutMs, records.data(), limit, count)) {
		auto const status = lexxos::devices::espidf_wifi::status();
		context.writeError("wifi-scan: failed: ");
		context.writeErrorLine(status.lastErrorName[0] != '\0' ? status.lastErrorName : "unknown");
		return 1;
	}
	context.write("networks: ");
	writeUnsigned(context, static_cast<std::uint32_t>(count));
	context.writeLine("");
	bool const showBssid = arguments.has("bssid");
	for (std::size_t index = 0; index < count; ++index) {
		writeEspIdfScanRecord(context, records[index], showBssid);
	}
	return 0;
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	return lexxos::apps::network_programs::pico_detail::runWifiScanOnPico(context, arguments, board);
#else
	(void) arguments;
	context.writeLine("wifi-scan: Wi-Fi support is unavailable in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::network_programs::detail
