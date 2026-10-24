#include <lexxos/devices/espidf/espidf_wifi.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <esp_err.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

namespace lexxos::devices::espidf_wifi {
namespace {

	constexpr std::uint32_t pollIntervalMs = 100;
	constexpr std::uint32_t reconnectIntervalMs = 1500;

	struct Runtime {
		esp_netif_t* netif{nullptr};
		Status status{};
		bool eventHandlersRegistered{false};
		bool scanActive{false};
		bool scanDone{false};
		bool connectInProgress{false};
		std::uint32_t lastConnectAttemptMs{0};
	};

	Runtime g_runtime{};

	std::uint32_t ticksToMs(TickType_t ticks) {
		return static_cast<std::uint32_t>(ticks * portTICK_PERIOD_MS);
	}

	std::uint32_t nowMs() {
		return ticksToMs(xTaskGetTickCount());
	}

	void delayMs(std::uint32_t milliseconds) {
		vTaskDelay(pdMS_TO_TICKS(milliseconds));
	}

	void setError(esp_err_t result) {
		g_runtime.status.lastError = result;
		auto const* name = esp_err_to_name(result);
		std::snprintf(g_runtime.status.lastErrorName, sizeof(g_runtime.status.lastErrorName), "%s", name != nullptr ? name : "ESP_ERR_UNKNOWN");
	}

	void clearError() {
		setError(ESP_OK);
	}

	void clearIp() {
		g_runtime.status.gotIp = false;
		g_runtime.status.ip[0] = '\0';
		g_runtime.status.netmask[0] = '\0';
		g_runtime.status.gateway[0] = '\0';
	}

	void clearDns() {
		g_runtime.status.dns0[0] = '\0';
		g_runtime.status.dns1[0] = '\0';
	}

	void clearLink() {
		g_runtime.status.connected = false;
		g_runtime.status.rssiValid = false;
		g_runtime.status.rssi = 0;
		g_runtime.status.signalLevel = 0;
		g_runtime.status.ssid[0] = '\0';
		clearIp();
		clearDns();
	}

	char uppercaseHexDigit(std::uint8_t value) {
		return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
	}

	std::uint8_t signalLevelFromRssi(std::int32_t rssi) {
		if (rssi >= -55) {
			return 3;
		}
		if (rssi >= -70) {
			return 2;
		}
		if (rssi >= -85) {
			return 1;
		}
		return 0;
	}

	void refreshHostnameFromMac() {
		if (!g_runtime.status.macValid) {
			g_runtime.status.hostname[0] = '\0';
			return;
		}
		auto const byte4 = g_runtime.status.mac[4];
		auto const byte5 = g_runtime.status.mac[5];
		std::snprintf(g_runtime.status.hostname,
		    sizeof(g_runtime.status.hostname),
		    "lexxos-%c%c%c%c",
		    uppercaseHexDigit(static_cast<std::uint8_t>((byte4 >> 4U) & 0x0FU)),
		    uppercaseHexDigit(static_cast<std::uint8_t>(byte4 & 0x0FU)),
		    uppercaseHexDigit(static_cast<std::uint8_t>((byte5 >> 4U) & 0x0FU)),
		    uppercaseHexDigit(static_cast<std::uint8_t>(byte5 & 0x0FU)));
	}

	void refreshMac() {
		auto result = esp_wifi_get_mac(WIFI_IF_STA, g_runtime.status.mac);
		if (result != ESP_OK) {
			result = esp_read_mac(g_runtime.status.mac, ESP_MAC_WIFI_STA);
		}
		g_runtime.status.macValid = result == ESP_OK;
		refreshHostnameFromMac();
	}

	void copySsid(char (&output)[33], std::uint8_t const* input, std::size_t inputSize) {
		std::size_t length = 0;
		while (length < inputSize && length < 32U && input[length] != '\0') {
			output[length] = static_cast<char>(input[length]);
			++length;
		}
		output[length] = '\0';
	}

	void copyString(char* output, std::size_t outputSize, char const* input) {
		if (output == nullptr || outputSize == 0U) {
			return;
		}
		std::snprintf(output, outputSize, "%s", input != nullptr ? input : "");
	}

	void writeDnsAddress(esp_netif_dns_type_t type, char (&output)[16]) {
		output[0] = '\0';
		if (g_runtime.netif == nullptr) {
			return;
		}
		esp_netif_dns_info_t dns{};
		if (esp_netif_get_dns_info(g_runtime.netif, type, &dns) != ESP_OK
		    || dns.ip.type != ESP_IPADDR_TYPE_V4
		    || dns.ip.u_addr.ip4.addr == 0) {
			return;
		}
		esp_ip4addr_ntoa(&dns.ip.u_addr.ip4, output, sizeof(output));
	}

	void refreshIpDetails() {
		if (g_runtime.netif == nullptr) {
			clearIp();
			clearDns();
			return;
		}

		esp_netif_ip_info_t ipInfo{};
		if (esp_netif_get_ip_info(g_runtime.netif, &ipInfo) == ESP_OK && ipInfo.ip.addr != 0) {
			g_runtime.status.gotIp = true;
			esp_ip4addr_ntoa(&ipInfo.ip, g_runtime.status.ip, sizeof(g_runtime.status.ip));
			esp_ip4addr_ntoa(&ipInfo.netmask, g_runtime.status.netmask, sizeof(g_runtime.status.netmask));
			esp_ip4addr_ntoa(&ipInfo.gw, g_runtime.status.gateway, sizeof(g_runtime.status.gateway));
			writeDnsAddress(ESP_NETIF_DNS_MAIN, g_runtime.status.dns0);
			writeDnsAddress(ESP_NETIF_DNS_BACKUP, g_runtime.status.dns1);
		} else {
			clearIp();
			clearDns();
		}
	}

	void refreshApInfo() {
		if (!g_runtime.status.initialized || !g_runtime.status.started) {
			g_runtime.status.rssiValid = false;
			g_runtime.status.signalLevel = 0;
			return;
		}

		wifi_ap_record_t apInfo{};
		if (esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK) {
			g_runtime.status.connected = true;
			copySsid(g_runtime.status.ssid, apInfo.ssid, sizeof(apInfo.ssid));
			g_runtime.status.rssiValid = true;
			g_runtime.status.rssi = apInfo.rssi;
			g_runtime.status.signalLevel = signalLevelFromRssi(apInfo.rssi);
		} else if (!g_runtime.status.connected) {
			g_runtime.status.rssiValid = false;
			g_runtime.status.signalLevel = 0;
		}
	}

	bool ensureNvs() {
		auto result = nvs_flash_init();
		if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
			auto const eraseResult = nvs_flash_erase();
			if (eraseResult != ESP_OK) {
				setError(eraseResult);
				return false;
			}
			result = nvs_flash_init();
		}
		if (result != ESP_OK) {
			setError(result);
			return false;
		}
		return true;
	}

	bool ensureNetworkCore() {
		auto result = esp_netif_init();
		if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
			setError(result);
			return false;
		}

		result = esp_event_loop_create_default();
		if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
			setError(result);
			return false;
		}
		return true;
	}

	wifi_auth_mode_t espAuthMode(AuthMode mode) {
		switch (mode) {
			case AuthMode::Open:
				return WIFI_AUTH_OPEN;
			case AuthMode::Wpa:
				return WIFI_AUTH_WPA_PSK;
			case AuthMode::Wpa2:
				return WIFI_AUTH_WPA2_PSK;
			case AuthMode::Mixed:
				return WIFI_AUTH_WPA_WPA2_PSK;
			case AuthMode::Auto:
				return WIFI_AUTH_WPA_PSK;
		}
		return WIFI_AUTH_WPA_PSK;
	}

	char const* scanAuthModeName(wifi_auth_mode_t mode) {
		switch (mode) {
			case WIFI_AUTH_OPEN:
				return "open";
			case WIFI_AUTH_WEP:
				return "wep";
			case WIFI_AUTH_WPA_PSK:
				return "wpa";
			case WIFI_AUTH_WPA2_PSK:
				return "wpa2";
			case WIFI_AUTH_WPA_WPA2_PSK:
				return "mixed";
			case WIFI_AUTH_ENTERPRISE:
				return "enterprise";
			case WIFI_AUTH_WPA3_PSK:
				return "wpa3";
			case WIFI_AUTH_WPA2_WPA3_PSK:
				return "wpa2-wpa3";
			case WIFI_AUTH_WAPI_PSK:
				return "wapi";
			case WIFI_AUTH_OWE:
				return "owe";
			case WIFI_AUTH_WPA3_ENT_192:
				return "wpa3-ent-192";
			case WIFI_AUTH_DPP:
				return "dpp";
			case WIFI_AUTH_WPA3_ENTERPRISE:
				return "wpa3-enterprise";
			case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
				return "wpa2-wpa3-enterprise";
			case WIFI_AUTH_WPA_ENTERPRISE:
				return "wpa-enterprise";
			default:
				return "unknown";
		}
	}

	void wifiEventHandler(void*, esp_event_base_t, std::int32_t eventId, void* eventData) {
		switch (eventId) {
			case WIFI_EVENT_SCAN_DONE:
				g_runtime.scanActive = false;
				g_runtime.scanDone = true;
				break;
			case WIFI_EVENT_STA_START:
				g_runtime.status.started = true;
				break;
			case WIFI_EVENT_STA_STOP:
				g_runtime.status.started = false;
				g_runtime.connectInProgress = false;
				clearLink();
				break;
			case WIFI_EVENT_STA_CONNECTED: {
				g_runtime.status.connected = true;
				g_runtime.status.lastDisconnectReason = 0;
				auto* event = static_cast<wifi_event_sta_connected_t*>(eventData);
				if (event != nullptr) {
					copySsid(g_runtime.status.ssid, event->ssid, event->ssid_len);
				}
				break;
			}
			case WIFI_EVENT_STA_DISCONNECTED: {
				auto* event = static_cast<wifi_event_sta_disconnected_t*>(eventData);
				g_runtime.status.lastDisconnectReason = event != nullptr ? event->reason : 0;
				clearLink();
				break;
			}
			default:
				break;
		}
	}

	void gotIpEventHandler(void*, esp_event_base_t, std::int32_t eventId, void* eventData) {
		if (eventId == IP_EVENT_STA_LOST_IP) {
			clearIp();
			return;
		}
		auto* event = static_cast<ip_event_got_ip_t*>(eventData);
		if (event == nullptr) {
			return;
		}
		g_runtime.status.gotIp = true;
		esp_ip4addr_ntoa(&event->ip_info.ip, g_runtime.status.ip, sizeof(g_runtime.status.ip));
		esp_ip4addr_ntoa(&event->ip_info.netmask, g_runtime.status.netmask, sizeof(g_runtime.status.netmask));
		esp_ip4addr_ntoa(&event->ip_info.gw, g_runtime.status.gateway, sizeof(g_runtime.status.gateway));
		writeDnsAddress(ESP_NETIF_DNS_MAIN, g_runtime.status.dns0);
		writeDnsAddress(ESP_NETIF_DNS_BACKUP, g_runtime.status.dns1);
		g_runtime.connectInProgress = false;
	}

	bool installDriver(lexxos::platform::BoardDescriptor const& board) {
		if (!board.wireless.wifi) {
			g_runtime.status = {};
			g_runtime.status.supported = false;
			setError(ESP_ERR_NOT_SUPPORTED);
			return false;
		}

		g_runtime.status.supported = true;
		if (g_runtime.status.initialized) {
			return true;
		}
		if (!ensureNvs() || !ensureNetworkCore()) {
			return false;
		}

		g_runtime.netif = esp_netif_create_default_wifi_sta();
		if (g_runtime.netif == nullptr) {
			setError(ESP_ERR_NO_MEM);
			return false;
		}

		wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
		auto result = esp_wifi_init(&config);
		if (result != ESP_OK) {
			setError(result);
			return false;
		}

		result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
		if (result != ESP_OK) {
			setError(result);
			return false;
		}

		result = esp_wifi_set_mode(WIFI_MODE_STA);
		if (result != ESP_OK) {
			setError(result);
			return false;
		}
		g_runtime.status.staModeEnabled = true;

		refreshMac();
		if (g_runtime.status.hostname[0] != '\0') {
			result = esp_netif_set_hostname(g_runtime.netif, g_runtime.status.hostname);
			if (result != ESP_OK) {
				setError(result);
				return false;
			}
		}

		if (!g_runtime.eventHandlersRegistered) {
			result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, nullptr);
			if (result != ESP_OK) {
				setError(result);
				return false;
			}
			result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &gotIpEventHandler, nullptr);
			if (result != ESP_OK) {
				setError(result);
				return false;
			}
			result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &gotIpEventHandler, nullptr);
			if (result != ESP_OK) {
				setError(result);
				return false;
			}
			g_runtime.eventHandlersRegistered = true;
		}

		g_runtime.status.initialized = true;
		clearError();
		return true;
	}

	bool copyCredential(std::string_view value, std::uint8_t* output, std::size_t capacity) {
		if (value.size() > capacity) {
			return false;
		}
		std::memset(output, 0, capacity);
		if (!value.empty()) {
			std::memcpy(output, value.data(), value.size());
		}
		return true;
	}

} // namespace

char const* toString(AuthMode mode) {
	switch (mode) {
		case AuthMode::Auto:
			return "auto";
		case AuthMode::Open:
			return "open";
		case AuthMode::Wpa:
			return "wpa";
		case AuthMode::Wpa2:
			return "wpa2";
		case AuthMode::Mixed:
			return "mixed";
	}
	return "unknown";
}

bool parseAuthMode(std::string_view text, AuthMode& output) {
	if (text == "auto" || text.empty()) {
		output = AuthMode::Auto;
		return true;
	}
	if (text == "open") {
		output = AuthMode::Open;
		return true;
	}
	if (text == "wpa") {
		output = AuthMode::Wpa;
		return true;
	}
	if (text == "wpa2") {
		output = AuthMode::Wpa2;
		return true;
	}
	if (text == "mixed") {
		output = AuthMode::Mixed;
		return true;
	}
	return false;
}

bool start(lexxos::platform::BoardDescriptor const& board) {
	if (!installDriver(board)) {
		return false;
	}
	if (g_runtime.status.started) {
		clearError();
		return true;
	}
	auto const result = esp_wifi_start();
	if (result != ESP_OK && result != ESP_ERR_WIFI_CONN) {
		setError(result);
		return false;
	}
	g_runtime.status.started = true;
	clearError();
	return true;
}

bool connect(
    lexxos::platform::BoardDescriptor const& board,
    std::string_view ssid,
    std::string_view password,
    AuthMode auth,
    std::uint32_t timeoutMs) {
	if (ssid.empty() || ssid.size() > 32U || password.size() > 64U || timeoutMs == 0U) {
		setError(ESP_ERR_INVALID_ARG);
		return false;
	}
	if (auth != AuthMode::Open && password.size() < 8U) {
		setError(ESP_ERR_INVALID_ARG);
		return false;
	}
	if (!start(board)) {
		return false;
	}

	wifi_config_t config{};
	if (!copyCredential(ssid, config.sta.ssid, sizeof(config.sta.ssid))
	    || !copyCredential(password, config.sta.password, sizeof(config.sta.password))) {
		setError(ESP_ERR_INVALID_ARG);
		return false;
	}
	config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
	config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
	config.sta.threshold.authmode = espAuthMode(auth);
	config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
	config.sta.failure_retry_cnt = 2;

	(void) esp_wifi_disconnect();
	clearLink();
	g_runtime.status.lastDisconnectReason = 0;

	auto result = esp_wifi_set_config(WIFI_IF_STA, &config);
	if (result != ESP_OK) {
		setError(result);
		return false;
	}

	result = esp_wifi_connect();
	if (result != ESP_OK && result != ESP_ERR_WIFI_CONN) {
		setError(result);
		return false;
	}
	g_runtime.connectInProgress = true;
	g_runtime.lastConnectAttemptMs = nowMs();

	auto const startMs = nowMs();
	while (static_cast<std::uint32_t>(nowMs() - startMs) < timeoutMs) {
		refreshIpDetails();
		refreshApInfo();
		if (g_runtime.status.connected && g_runtime.status.gotIp) {
			g_runtime.connectInProgress = false;
			clearError();
			return true;
		}
		if (!g_runtime.status.connected && static_cast<std::uint32_t>(nowMs() - g_runtime.lastConnectAttemptMs) >= reconnectIntervalMs) {
			auto const retryResult = esp_wifi_connect();
			if (retryResult == ESP_OK || retryResult == ESP_ERR_WIFI_CONN) {
				g_runtime.lastConnectAttemptMs = nowMs();
			} else {
				setError(retryResult);
			}
		}
		delayMs(pollIntervalMs);
	}

	g_runtime.connectInProgress = false;
	setError(ESP_ERR_TIMEOUT);
	return false;
}

bool disconnect() {
	if (!g_runtime.status.initialized) {
		return true;
	}
	auto const result = esp_wifi_disconnect();
	if (result != ESP_OK && result != ESP_ERR_WIFI_NOT_CONNECT) {
		setError(result);
		return false;
	}
	clearLink();
	clearError();
	return true;
}

bool scan(
    lexxos::platform::BoardDescriptor const& board,
    std::uint32_t timeoutMs,
    ScanRecord* records,
    std::size_t capacity,
    std::size_t& count) {
	count = 0;
	if (records == nullptr || capacity == 0U || timeoutMs == 0U) {
		setError(ESP_ERR_INVALID_ARG);
		return false;
	}
	if (!board.wireless.scan) {
		setError(ESP_ERR_NOT_SUPPORTED);
		return false;
	}
	if (!start(board)) {
		return false;
	}
	if (g_runtime.scanActive) {
		setError(ESP_ERR_INVALID_STATE);
		return false;
	}

	wifi_scan_config_t scanConfig{};
	scanConfig.scan_type = WIFI_SCAN_TYPE_ACTIVE;
	scanConfig.scan_time.active.min = 60;
	scanConfig.scan_time.active.max = 120;

	g_runtime.scanActive = true;
	g_runtime.scanDone = false;
	auto result = esp_wifi_scan_start(&scanConfig, false);
	if (result != ESP_OK) {
		g_runtime.scanActive = false;
		setError(result);
		return false;
	}

	auto const startedMs = nowMs();
	while (!g_runtime.scanDone) {
		if (static_cast<std::uint32_t>(nowMs() - startedMs) >= timeoutMs) {
			(void) esp_wifi_scan_stop();
			g_runtime.scanActive = false;
			setError(ESP_ERR_TIMEOUT);
			return false;
		}
		delayMs(pollIntervalMs);
	}
	g_runtime.scanActive = false;

	std::uint16_t apCount = 0;
	result = esp_wifi_scan_get_ap_num(&apCount);
	if (result != ESP_OK) {
		setError(result);
		return false;
	}

	std::uint16_t number = static_cast<std::uint16_t>(std::min<std::size_t>({capacity, maxWifiScanResults, apCount}));
	wifi_ap_record_t apRecords[maxWifiScanResults]{};
	result = esp_wifi_scan_get_ap_records(&number, apRecords);
	if (result != ESP_OK) {
		setError(result);
		return false;
	}

	std::size_t const outputCount = std::min<std::size_t>(number, capacity);
	for (std::size_t index = 0; index < outputCount; ++index) {
		auto const& source = apRecords[index];
		auto& destination = records[index];
		destination = {};
		copySsid(destination.ssid, source.ssid, sizeof(source.ssid));
		std::memcpy(destination.bssid, source.bssid, sizeof(destination.bssid));
		destination.bssidValid = true;
		destination.rssi = source.rssi;
		destination.channel = source.primary;
		copyString(destination.auth, sizeof(destination.auth), scanAuthModeName(source.authmode));
	}
	count = outputCount;
	clearError();
	return true;
}

Status status() {
	if (g_runtime.status.initialized) {
		refreshMac();
		refreshIpDetails();
		refreshApInfo();
	}
	return g_runtime.status;
}

} // namespace lexxos::devices::espidf_wifi
