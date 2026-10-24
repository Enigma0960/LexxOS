#include <lexxos/apps/network/detail/pico_network_support.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/network/detail/pico_wifi_scan_types.h>
#include <lexxos/platform/platform_services.h>
#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE)
	#include <pico/cyw43_arch.h>
#endif

#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	#include <lwip/dhcp.h>
	#include <lwip/dns.h>
	#include <lwip/icmp.h>
	#include <lwip/inet_chksum.h>
	#include <lwip/ip4_addr.h>
	#include <lwip/ip_addr.h>
	#include <lwip/netif.h>
	#include <lwip/pbuf.h>
	#include <lwip/prot/ip.h>
	#include <lwip/raw.h>
	#include <lwip/udp.h>
#endif

namespace lexxos::apps::network_programs::pico_detail {
namespace {

	constexpr std::uint32_t wifiDefaultTimeoutMs = 10000;
	constexpr std::uint32_t wifiMaxTimeoutMs = 60000;
	constexpr std::uint32_t wifiConnectDefaultTimeoutMs = 30000;
	constexpr std::uint32_t wifiConnectMaxTimeoutMs = 120000;
	constexpr std::uint32_t wifiConnectStatusPollMs = 250;
	constexpr int wifiConnectTimeoutResult = -2;
	constexpr std::size_t wifiMaxPasswordLength = 64;
	constexpr std::uint32_t ntpUnixEpochDelta = 2208988800UL;
	constexpr std::size_t ntpPacketSize = 48;
	constexpr std::uint16_t ntpPort = 123;
	constexpr std::uint32_t ntpPollIntervalMs = 50;
	constexpr std::size_t ntpMaxServerNameLength = 127;
	constexpr std::uint32_t pingDefaultTimeoutMs = 5000;
	constexpr std::uint32_t pingMaxTimeoutMs = 60000;
	constexpr std::uint32_t pingDefaultCount = 4;
	constexpr std::uint32_t pingMaxCount = 10;
	constexpr std::size_t pingPayloadBytes = 32;
	constexpr std::uint16_t pingIdentifier = 0x4C58U;

	void writeSigned(lexxos::os::AppContext& context, int value) {
		if (value < 0) {
			context.write("-");
			writeUnsigned(context, static_cast<std::uint32_t>(-value));
			return;
		}
		writeUnsigned(context, static_cast<std::uint32_t>(value));
	}

	bool copyStringToBuffer(std::string_view text, char* output, std::size_t capacity) {
		if (output == nullptr || capacity == 0 || text.size() >= capacity) {
			return false;
		}
		for (std::size_t index = 0; index < text.size(); ++index) {
			output[index] = text[index];
		}
		output[text.size()] = '\0';
		return true;
	}

	std::uint32_t elapsedMs(std::uint32_t start, std::uint32_t now) {
		return now >= start ? now - start : 0;
	}

	std::string_view authName(std::uint8_t authMode) {
		constexpr std::uint8_t privacyBit = 0x01;
		constexpr std::uint8_t wpaBit = 0x02;
		constexpr std::uint8_t wpa2Bit = 0x04;
		if (authMode == 0) {
			return "open";
		}
		bool const hasWpa = (authMode & wpaBit) != 0;
		bool const hasWpa2 = (authMode & wpa2Bit) != 0;
		if (hasWpa && hasWpa2) {
			return "wpa/wpa2";
		}
		if (hasWpa2) {
			return "wpa2";
		}
		if (hasWpa) {
			return "wpa";
		}
		if ((authMode & privacyBit) != 0) {
			return "wep";
		}
		return "secure";
	}

	bool sameBssid(WifiBssid const& lhs, std::uint8_t const rhs[wifiBssidLength]) {
		return std::memcmp(lhs.data(), rhs, lhs.size()) == 0;
	}

	void writeBssid(lexxos::os::AppContext& context, WifiBssid const& bssid) {
		for (std::size_t index = 0; index < bssid.size(); ++index) {
			if (index > 0) {
				context.write(":");
			}
			writeHexByte(context, bssid[index]);
		}
	}

	std::uint8_t wifiSignalLevelFromRssi(std::int32_t rssi) {
		if (rssi >= -60) {
			return 3;
		}
		if (rssi >= -75) {
			return 2;
		}
		return 1;
	}

#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE)
	WifiScanState g_wifiScanState{};
	bool g_cyw43Initialized{false};
	int g_cyw43InitResult{0};
	bool g_wifiStaModeEnabled{false};
	std::uint16_t g_pingSequence{0};

	struct WifiAuthAttempt {
		std::string_view name{};
		std::uint32_t mode{0};
	};

	int wifiScanCallback(void* env, cyw43_ev_scan_result_t const* result) {
		auto* state = static_cast<WifiScanState*>(env);
		if (state == nullptr || result == nullptr || !state->armed) {
			return 0;
		}

		auto const ssidLength = result->ssid_len <= 32 ? result->ssid_len : 32;
		if (!state->raw) {
			for (std::uint32_t index = 0; index < state->count; ++index) {
				auto& record = state->results[index];
				if (record.ssidLength == ssidLength
				    && record.channel == result->channel
				    && record.authMode == result->auth_mode
				    && std::memcmp(record.ssid, result->ssid, ssidLength) == 0) {
					if (!sameBssid(record.bssid, result->bssid)) {
						record.multipleBssid = true;
					}
					if (result->rssi > record.rssi) {
						std::memcpy(record.bssid.data(), result->bssid, record.bssid.size());
						record.rssi = result->rssi;
					}
					++record.seen;
					++state->merged;
					return 0;
				}
			}
		}

		if (state->count >= state->results.size() || state->count >= state->limit) {
			++state->dropped;
			return 0;
		}

		auto& record = state->results[state->count++];
		record.ssidLength = ssidLength;
		for (std::uint8_t index = 0; index < ssidLength; ++index) {
			record.ssid[index] = static_cast<char>(result->ssid[index]);
		}
		record.ssid[ssidLength] = '\0';
		std::memcpy(record.bssid.data(), result->bssid, record.bssid.size());
		record.channel = result->channel;
		record.authMode = result->auth_mode;
		record.rssi = result->rssi;
		return 0;
	}

	bool ensureCyw43Initialized(lexxos::os::AppContext& context, std::string_view programName) {
		if (g_cyw43Initialized) {
			return true;
		}
		g_cyw43InitResult = cyw43_arch_init();
		if (g_cyw43InitResult != 0) {
			context.writeError(programName);
			context.writeError(": cyw43 init failed: ");
			writeSigned(context, g_cyw43InitResult);
			context.writeErrorLine("");
			return false;
		}
		g_cyw43Initialized = true;
		return true;
	}

	void ensureWifiStaModeEnabled() {
		if (!g_wifiStaModeEnabled) {
			cyw43_arch_enable_sta_mode();
			g_wifiStaModeEnabled = true;
		}
	}

	bool parseWifiAuth(std::string_view text, WifiAuthAttempt& output) {
		if (text == "wpa2") {
			output = WifiAuthAttempt{"wpa2", CYW43_AUTH_WPA2_AES_PSK};
			return true;
		}
		if (text == "open") {
			output = WifiAuthAttempt{"open", CYW43_AUTH_OPEN};
			return true;
		}
		if (text == "wpa") {
			output = WifiAuthAttempt{"wpa", CYW43_AUTH_WPA_TKIP_PSK};
			return true;
		}
		if (text == "mixed" || text == "wpa2-mixed" || text == "wpa/wpa2") {
			output = WifiAuthAttempt{"mixed", CYW43_AUTH_WPA2_MIXED_PSK};
			return true;
		}
		return false;
	}

	void writeWifiConnectFailure(
	    lexxos::os::AppContext& context,
	    std::string_view authName,
	    int result) {
		context.writeError("wifi-connect: auth=");
		context.writeError(authName);
		context.writeError(" failed: ");
		writeSigned(context, result);
		if (result == wifiConnectTimeoutResult) {
			context.writeError(" (timeout)");
		}
		context.writeErrorLine("");
	}

	void fillAutoWifiAuthAttempts(std::array<WifiAuthAttempt, 3>& attempts, std::size_t& count) {
		attempts[0] = WifiAuthAttempt{"wpa2", CYW43_AUTH_WPA2_AES_PSK};
		attempts[1] = WifiAuthAttempt{"wpa", CYW43_AUTH_WPA_TKIP_PSK};
		attempts[2] = WifiAuthAttempt{"mixed", CYW43_AUTH_WPA2_MIXED_PSK};
		count = 3;
	}

	#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	struct DnsQueryState {
		bool volatile done{false};
		bool success{false};
		ip_addr_t address{};
	};

	struct NtpQueryState {
		bool volatile done{false};
		bool success{false};
		std::array<std::uint8_t, ntpPacketSize> response{};
		std::uint16_t responseLength{0};
		udp_pcb* pcb{nullptr};
	};

	struct WifiStatusSnapshot {
		bool cyw43Initialized{false};
		int cyw43InitResult{0};
		bool staModeEnabled{false};
		bool netifValid{false};
		bool netifUp{false};
		bool netifLinkUp{false};
		std::uint8_t netifFlags{0};
		int linkStatus{CYW43_LINK_DOWN};
		std::array<std::uint8_t, 6> mac{};
		std::uint8_t macLength{0};
		ip4_addr_t ip{};
		ip4_addr_t netmask{};
		ip4_addr_t gateway{};
		ip_addr_t dns0{};
		ip_addr_t dns1{};
	};

	struct PingQueryState {
		bool volatile done{false};
		bool success{false};
		ip_addr_t address{};
		raw_pcb* pcb{nullptr};
		std::uint16_t identifier{pingIdentifier};
		std::uint16_t sequence{0};
		std::uint16_t responseLength{0};
	};

	enum class PingWaitResult : std::uint8_t {
		Reply,
		Timeout,
		Cancelled,
	};

	void dnsCallback(char const* name, ip_addr_t const* address, void* argument);

	bool waitForDns(
	    lexxos::os::AppContext& context,
	    DnsQueryState const& state,
	    std::uint32_t startMs,
	    std::uint32_t timeoutMs);

	std::string_view wifiLinkStatusName(int status) {
		switch (status) {
			case CYW43_LINK_DOWN:
				return "down";
			case CYW43_LINK_JOIN:
				return "join";
			case CYW43_LINK_NOIP:
				return "no-ip";
			case CYW43_LINK_UP:
				return "up";
			case CYW43_LINK_FAIL:
				return "fail";
			case CYW43_LINK_NONET:
				return "nonet";
			case CYW43_LINK_BADAUTH:
				return "badauth";
			default:
				return "unknown";
		}
	}

	bool wifiLinkStatusIsTerminalFailure(int status) {
		switch (status) {
			case CYW43_LINK_FAIL:
			case CYW43_LINK_NONET:
			case CYW43_LINK_BADAUTH:
				return true;
			default:
				return false;
		}
	}

	void writeWifiLinkStatus(lexxos::os::AppContext& context, int status) {
		context.write("wifi-connect: link=");
		context.write(wifiLinkStatusName(status));
		context.write(" (");
		writeSigned(context, status);
		context.writeLine(")");
	}

	void writeIp4Address(lexxos::os::AppContext& context, ip4_addr_t const& address) {
		char buffer[16]{};
		ip4addr_ntoa_r(&address, buffer, sizeof(buffer));
		context.write(buffer);
	}

	void writeIpAddress(lexxos::os::AppContext& context, ip_addr_t const& address) {
		char buffer[IPADDR_STRLEN_MAX]{};
		ipaddr_ntoa_r(&address, buffer, sizeof(buffer));
		context.write(buffer);
	}

	void writeMacAddress(lexxos::os::AppContext& context, WifiStatusSnapshot const& snapshot) {
		if (snapshot.macLength == 0) {
			context.write("-");
			return;
		}
		for (std::uint8_t index = 0; index < snapshot.macLength; ++index) {
			if (index > 0) {
				context.write(":");
			}
			writeHexByte(context, snapshot.mac[index]);
		}
	}

	void readWifiStatusSnapshot(WifiStatusSnapshot& snapshot) {
		snapshot = WifiStatusSnapshot{};
		snapshot.cyw43Initialized = g_cyw43Initialized;
		snapshot.cyw43InitResult = g_cyw43InitResult;
		snapshot.staModeEnabled = g_wifiStaModeEnabled;
		if (!g_cyw43Initialized) {
			return;
		}

		cyw43_arch_lwip_begin();
		snapshot.linkStatus = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
		auto* staNetif = &cyw43_state.netif[CYW43_ITF_STA];
		snapshot.netifValid = true;
		snapshot.netifUp = netif_is_up(staNetif);
		snapshot.netifLinkUp = netif_is_link_up(staNetif);
		snapshot.netifFlags = staNetif->flags;
		snapshot.ip = *netif_ip4_addr(staNetif);
		snapshot.netmask = *netif_ip4_netmask(staNetif);
		snapshot.gateway = *netif_ip4_gw(staNetif);
		snapshot.macLength = staNetif->hwaddr_len <= snapshot.mac.size()
		    ? static_cast<std::uint8_t>(staNetif->hwaddr_len)
		    : static_cast<std::uint8_t>(snapshot.mac.size());
		for (std::uint8_t index = 0; index < snapshot.macLength; ++index) {
			snapshot.mac[index] = staNetif->hwaddr[index];
		}
		auto const* dns0 = dns_getserver(0);
		auto const* dns1 = dns_getserver(1);
		if (dns0 != nullptr) {
			ip_addr_set(&snapshot.dns0, dns0);
		}
		if (dns1 != nullptr) {
			ip_addr_set(&snapshot.dns1, dns1);
		}
		cyw43_arch_lwip_end();
	}

	int readWifiLinkStatus() {
		cyw43_arch_lwip_begin();
		int const status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
		cyw43_arch_lwip_end();
		return status;
	}

	bool restartWifiDhcpClient(lexxos::os::AppContext& context) {
		bool restarted = false;
		err_t restartResult = ERR_IF;
		cyw43_arch_lwip_begin();
		auto* staNetif = &cyw43_state.netif[CYW43_ITF_STA];
		if ((staNetif->flags & NETIF_FLAG_UP) != 0) {
			dhcp_stop(staNetif);
			ip4_addr_t zero{};
			zero.addr = 0;
			netif_set_addr(staNetif, &zero, &zero, &zero);
			restartResult = dhcp_start(staNetif);
			restarted = restartResult == ERR_OK;
		}
		cyw43_arch_lwip_end();

		if (!restarted) {
			context.writeError("wifi-connect: DHCP restart failed: ");
			writeSigned(context, restartResult);
			context.writeErrorLine("");
			return false;
		}
		context.writeLine("wifi-connect: DHCP restart");
		return true;
	}

	bool resolveNetworkHost(lexxos::os::AppContext& context,
	    std::string_view programName,
	    char const* hostName,
	    std::uint32_t startMs,
	    std::uint32_t timeoutMs,
	    ip_addr_t& address) {
		if (ipaddr_aton(hostName, &address)) {
			return true;
		}

		DnsQueryState state{};
		context.write(programName);
		context.write(": resolving ");
		context.write(hostName);
		context.writeLine("");
		cyw43_arch_lwip_begin();
		err_t const result = dns_gethostbyname(hostName, &state.address, dnsCallback, &state);
		cyw43_arch_lwip_end();
		if (result == ERR_OK) {
			address = state.address;
			return true;
		}
		if (result != ERR_INPROGRESS) {
			context.writeError(programName);
			context.writeError(": DNS start failed: ");
			writeSigned(context, result);
			context.writeErrorLine("");
			return false;
		}
		if (!waitForDns(context, state, startMs, timeoutMs)) {
			context.writeError(programName);
			context.writeErrorLine(": DNS timeout or failure");
			return false;
		}
		address = state.address;
		return true;
	}

	bool pingEchoHeaderMatches(PingQueryState const& state, icmp_echo_hdr const& header) {
		return header.type == ICMP_ER
		    && header.id == lwip_htons(state.identifier)
		    && header.seqno == lwip_htons(state.sequence);
	}

	bool copyPingEchoHeader(pbuf* buffer, std::uint16_t offset, icmp_echo_hdr& header) {
		if (buffer == nullptr || buffer->tot_len < offset + sizeof(header)) {
			return false;
		}
		return pbuf_copy_partial(buffer, &header, sizeof(header), offset) == sizeof(header);
	}

	bool ipv4HeaderLength(pbuf* buffer, std::uint16_t& output) {
		std::uint8_t firstByte = 0;
		if (buffer == nullptr || pbuf_copy_partial(buffer, &firstByte, sizeof(firstByte), 0) != sizeof(firstByte)) {
			return false;
		}
		std::uint8_t const version = static_cast<std::uint8_t>(firstByte >> 4U);
		std::uint8_t const headerWords = static_cast<std::uint8_t>(firstByte & 0x0FU);
		if (version != 4U || headerWords < 5U) {
			return false;
		}
		output = static_cast<std::uint16_t>(headerWords * 4U);
		return true;
	}

	u8_t pingReceiveCallback(void* argument, raw_pcb*, pbuf* buffer, ip_addr_t const* address) {
		auto* state = static_cast<PingQueryState*>(argument);
		if (state == nullptr || buffer == nullptr || address == nullptr || !ip_addr_cmp(address, &state->address)) {
			return 0;
		}

		icmp_echo_hdr header{};

		std::uint16_t icmpOffset = 0;
		bool matched = copyPingEchoHeader(buffer, icmpOffset, header) && pingEchoHeaderMatches(*state, header);
		if (!matched) {
			std::uint16_t ipHeaderBytes = 0;
			if (!ipv4HeaderLength(buffer, ipHeaderBytes)) {
				return 0;
			}
			icmpOffset = ipHeaderBytes;
			matched = copyPingEchoHeader(buffer, icmpOffset, header) && pingEchoHeaderMatches(*state, header);
			if (!matched) {
				return 0;
			}
		}

		state->responseLength = buffer->tot_len > icmpOffset
		    ? static_cast<std::uint16_t>(buffer->tot_len - icmpOffset)
		    : buffer->tot_len;
		state->success = true;
		state->done = true;
		pbuf_free(buffer);
		return 1;
	}

	bool startPingRequest(lexxos::os::AppContext& context, PingQueryState& state) {
		char const* errorMessage = nullptr;
		err_t result = ERR_OK;

		cyw43_arch_lwip_begin();
		state.pcb = raw_new(IP_PROTO_ICMP);
		if (state.pcb == nullptr) {
			errorMessage = "failed to allocate raw PCB";
		} else {
			raw_recv(state.pcb, pingReceiveCallback, &state);
			(void) raw_bind(state.pcb, IP_ADDR_ANY);

			auto* packet = pbuf_alloc(PBUF_IP, sizeof(icmp_echo_hdr) + pingPayloadBytes, PBUF_RAM);
			if (packet == nullptr) {
				errorMessage = "failed to allocate ICMP packet";
			} else {
				auto* echo = static_cast<icmp_echo_hdr*>(packet->payload);
				ICMPH_TYPE_SET(echo, ICMP_ECHO);
				ICMPH_CODE_SET(echo, 0);
				echo->chksum = 0;
				echo->id = lwip_htons(state.identifier);
				echo->seqno = lwip_htons(state.sequence);
				auto* payload = static_cast<std::uint8_t*>(packet->payload) + sizeof(icmp_echo_hdr);
				for (std::size_t index = 0; index < pingPayloadBytes; ++index) {
					payload[index] = static_cast<std::uint8_t>('a' + (index % 26U));
				}
				echo->chksum = inet_chksum(echo, packet->len);
				result = raw_sendto(state.pcb, packet, &state.address);
				pbuf_free(packet);
				if (result != ERR_OK) {
					errorMessage = "ICMP send failed";
				}
			}
		}
		if (errorMessage != nullptr && state.pcb != nullptr) {
			raw_remove(state.pcb);
			state.pcb = nullptr;
		}
		cyw43_arch_lwip_end();

		if (errorMessage == nullptr) {
			return true;
		}
		context.writeError("ping: ");
		context.writeError(errorMessage);
		if (result != ERR_OK) {
			context.writeError(": ");
			writeSigned(context, result);
		}
		context.writeErrorLine("");
		return false;
	}

	void closePingRequest(PingQueryState& state) {
		if (state.pcb == nullptr) {
			return;
		}
		cyw43_arch_lwip_begin();
		raw_remove(state.pcb);
		state.pcb = nullptr;
		cyw43_arch_lwip_end();
	}

	PingWaitResult waitForPing(lexxos::os::AppContext& context, PingQueryState const& state, std::uint32_t startMs, std::uint32_t timeoutMs) {
		while (!state.done) {
			if (context.pollCancellationRequest()) {
				return PingWaitResult::Cancelled;
			}

			if (timeoutMs > 0 && elapsedMs(startMs, uptimeMs(context)) >= timeoutMs) {
				return PingWaitResult::Timeout;
			}

			cyw43_arch_poll();
			sleepMs(context, 20);
		}
		return state.success ? PingWaitResult::Reply : PingWaitResult::Timeout;
	}

	bool sleepPingInterval(lexxos::os::AppContext& context, std::uint32_t durationMs) {
		auto const startMs = uptimeMs(context);
		while (elapsedMs(startMs, uptimeMs(context)) < durationMs) {
			if (context.pollCancellationRequest()) {
				return false;
			}
			sleepMs(context, 50);
		}
		return true;
	}

	void prepareWifiStationForConnect(lexxos::os::AppContext& context) {
		cyw43_arch_lwip_begin();
		cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
		#if defined(CYW43_NO_POWERSAVE_MODE)
		(void) cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);
		#endif
		cyw43_arch_lwip_end();
		sleepMs(context, wifiConnectStatusPollMs);
	}

	bool connectWifiWithStatusTrace(
	    lexxos::os::AppContext& context,
	    char const* ssid,
	    char const* password,
	    WifiAuthAttempt const& attempt,
	    std::uint32_t timeoutMs,
	    int& result) {
		prepareWifiStationForConnect(context);

		result = cyw43_arch_wifi_connect_async(ssid, password, attempt.mode);
		if (result != 0) {
			return false;
		}

		auto const startMs = uptimeMs(context);
		int lastStatus = std::numeric_limits<int>::min();
		bool dhcpRestarted = false;

		while (elapsedMs(startMs, uptimeMs(context)) < timeoutMs) {
			if (context.pollCancellationRequest()) {
				result = wifiConnectTimeoutResult;
				return false;
			}

			int const status = readWifiLinkStatus();
			if (status != lastStatus) {
				writeWifiLinkStatus(context, status);
				lastStatus = status;
			}

			if (status == CYW43_LINK_UP) {
				result = 0;
				return true;
			}

			if (status == CYW43_LINK_NOIP && !dhcpRestarted) {
				dhcpRestarted = true;
				(void) restartWifiDhcpClient(context);
			}

			if (wifiLinkStatusIsTerminalFailure(status)) {
				result = status;
				return false;
			}

			cyw43_arch_poll();
			sleepMs(context, wifiConnectStatusPollMs);
		}

		result = wifiConnectTimeoutResult;
		return false;
	}

	void dnsCallback(char const*, ip_addr_t const* address, void* argument) {
		auto* state = static_cast<DnsQueryState*>(argument);
		if (state == nullptr) {
			return;
		}
		if (address != nullptr) {
			ip_addr_set(&state->address, address);
			state->success = true;
		}
		state->done = true;
	}

	void ntpReceiveCallback(void* argument, udp_pcb*, pbuf* buffer, ip_addr_t const*, u16_t) {
		auto* state = static_cast<NtpQueryState*>(argument);
		if (state == nullptr) {
			if (buffer != nullptr) {
				pbuf_free(buffer);
			}
			return;
		}
		if (buffer != nullptr) {
			state->responseLength = buffer->tot_len;
			if (buffer->tot_len >= ntpPacketSize) {
				pbuf_copy_partial(buffer, state->response.data(), ntpPacketSize, 0);
				state->success = true;
			}
			pbuf_free(buffer);
		}
		state->done = true;
	}

	bool waitForDns(lexxos::os::AppContext& context, DnsQueryState const& state, std::uint32_t startMs, std::uint32_t timeoutMs) {
		while (!state.done) {
			if (timeoutMs > 0 && elapsedMs(startMs, uptimeMs(context)) >= timeoutMs) {
				return false;
			}
			sleepMs(context, ntpPollIntervalMs);
		}
		return state.success;
	}

	bool waitForNtp(lexxos::os::AppContext& context, NtpQueryState const& state, std::uint32_t startMs, std::uint32_t timeoutMs) {
		while (!state.done) {
			if (timeoutMs > 0 && elapsedMs(startMs, uptimeMs(context)) >= timeoutMs) {
				return false;
			}
			sleepMs(context, ntpPollIntervalMs);
		}
		return state.success;
	}

	bool wifiLinkIsUp() {
		int const status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
		return status == CYW43_LINK_UP;
	}

	bool resolveNtpServer(lexxos::os::AppContext& context,
	    char const* serverName,
	    std::uint32_t startMs,
	    std::uint32_t timeoutMs,
	    ip_addr_t& address) {
		return resolveNetworkHost(context, "rtc-sync", serverName, startMs, timeoutMs, address);
	}

	bool startNtpRequest(lexxos::os::AppContext& context, ip_addr_t const& address, NtpQueryState& state) {
		std::array<std::uint8_t, ntpPacketSize> request{};
		request[0] = 0x1B;

		cyw43_arch_lwip_begin();
		auto* pcb = udp_new();
		if (pcb == nullptr) {
			cyw43_arch_lwip_end();
			context.writeErrorLine("rtc-sync: failed to allocate UDP PCB");
			return false;
		}
		udp_recv(pcb, ntpReceiveCallback, &state);
		state.pcb = pcb;

		auto* packet = pbuf_alloc(PBUF_TRANSPORT, ntpPacketSize, PBUF_RAM);
		if (packet == nullptr) {
			udp_remove(pcb);
			state.pcb = nullptr;
			cyw43_arch_lwip_end();
			context.writeErrorLine("rtc-sync: failed to allocate NTP packet");
			return false;
		}
		std::memcpy(packet->payload, request.data(), request.size());
		err_t const result = udp_sendto(pcb, packet, &address, ntpPort);
		pbuf_free(packet);
		if (result != ERR_OK) {
			udp_remove(pcb);
			state.pcb = nullptr;
			cyw43_arch_lwip_end();
			context.writeError("rtc-sync: UDP send failed: ");
			writeSigned(context, result);
			context.writeErrorLine("");
			return false;
		}
		cyw43_arch_lwip_end();
		return true;
	}

	void closeNtpRequest(NtpQueryState& state) {
		if (state.pcb == nullptr) {
			return;
		}
		cyw43_arch_lwip_begin();
		udp_remove(state.pcb);
		state.pcb = nullptr;
		cyw43_arch_lwip_end();
	}

	std::uint32_t readBe32(std::array<std::uint8_t, ntpPacketSize> const& data, std::size_t offset) {
		return (static_cast<std::uint32_t>(data[offset]) << 24U)
		    | (static_cast<std::uint32_t>(data[offset + 1U]) << 16U)
		    | (static_cast<std::uint32_t>(data[offset + 2U]) << 8U)
		    | static_cast<std::uint32_t>(data[offset + 3U]);
	}

	void unixSecondsToUtc(std::uint32_t unixSeconds, lexxos::devices::rtc::DateTime& output) {
		std::int64_t const daysSinceUnixEpoch = static_cast<std::int64_t>(unixSeconds / 86400U);
		std::uint32_t const secondsOfDay = unixSeconds % 86400U;
		std::int64_t z = daysSinceUnixEpoch + 719468;
		std::int64_t const era = (z >= 0 ? z : z - 146096) / 146097;
		std::uint64_t const dayOfEra = static_cast<std::uint64_t>(z - era * 146097);
		std::uint64_t const yearOfEra = (dayOfEra - dayOfEra / 1460U + dayOfEra / 36524U - dayOfEra / 146096U) / 365U;
		std::int64_t year = static_cast<std::int64_t>(yearOfEra) + era * 400;
		std::uint64_t const dayOfYear = dayOfEra - (365U * yearOfEra + yearOfEra / 4U - yearOfEra / 100U);
		std::uint64_t const monthPrime = (5U * dayOfYear + 2U) / 153U;
		std::uint64_t const day = dayOfYear - (153U * monthPrime + 2U) / 5U + 1U;
		std::uint64_t const month = monthPrime < 10U ? monthPrime + 3U : monthPrime - 9U;
		year += month <= 2U ? 1 : 0;

		output.year = static_cast<std::uint16_t>(year);
		output.month = static_cast<std::uint8_t>(month);
		output.day = static_cast<std::uint8_t>(day);
		output.hour = static_cast<std::uint8_t>(secondsOfDay / 3600U);
		output.minute = static_cast<std::uint8_t>((secondsOfDay / 60U) % 60U);
		output.second = static_cast<std::uint8_t>(secondsOfDay % 60U);
		output.weekday = lexxos::devices::rtc::weekdayIso(output);
	}

	bool decodeNtpTime(NtpQueryState const& state, lexxos::devices::rtc::DateTime& output, std::uint32_t& unixSeconds) {
		if (!state.success || state.responseLength < ntpPacketSize) {
			return false;
		}
		std::uint32_t const ntpSeconds = readBe32(state.response, 40U);
		if (ntpSeconds < ntpUnixEpochDelta) {
			return false;
		}
		unixSeconds = ntpSeconds - ntpUnixEpochDelta;
		unixSecondsToUtc(unixSeconds, output);
		return lexxos::devices::rtc::isValidDate(output);
	}
	#endif

	bool wifiScanActive() {
		cyw43_arch_lwip_begin();
		bool const active = cyw43_wifi_scan_active(&cyw43_state);
		cyw43_arch_lwip_end();
		return active;
	}

	void resetWifiScanState(WifiScanState& state, std::uint32_t limit, bool raw, bool showBssid) {
		state = WifiScanState{};
		state.limit = limit;
		state.raw = raw;
		state.showBssid = showBssid;
		state.armed = true;
	}

	void writeWifiScanResults(lexxos::os::AppContext& context, WifiScanState const& state) {
		context.write("networks: ");
		writeUnsigned(context, state.count);
		if (state.merged > 0) {
			context.write(" merged=");
			writeUnsigned(context, state.merged);
		}
		if (state.dropped > 0) {
			context.write(" dropped=");
			writeUnsigned(context, state.dropped);
		}
		context.writeLine("");

		for (std::uint32_t index = 0; index < state.count; ++index) {
			auto const& record = state.results[index];
			writeUnsigned(context, index + 1);
			context.write(": ssid=");
			if (record.ssidLength == 0) {
				context.write("<hidden>");
			} else {
				context.write(record.ssid);
			}
			if (state.showBssid) {
				context.write(" bssid=");
				writeBssid(context, record.bssid);
				if (record.multipleBssid) {
					context.write(" bssid-multi=yes");
				}
			}
			context.write(" ch=");
			writeUnsigned(context, record.channel);
			context.write(" rssi=");
			writeSigned(context, record.rssi);
			context.write(" auth=");
			context.write(authName(record.authMode));
			if (record.seen > 1) {
				context.write(" seen=");
				writeUnsigned(context, record.seen);
			}
			context.writeLine("");
		}
	}
#endif

} // namespace

WifiRuntimeStatus readWifiRuntimeStatusOnPico() {
	WifiRuntimeStatus output{};
#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	output.supported = true;
	output.initialized = g_cyw43Initialized;
	output.staModeEnabled = g_wifiStaModeEnabled;
	if (!g_cyw43Initialized) {
		return output;
	}

	cyw43_arch_lwip_begin();
	auto const linkStatus = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
	output.connected = linkStatus == CYW43_LINK_UP;
	if (output.connected) {
		std::int32_t rssi = 0;
		if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) == 0) {
			output.rssiValid = true;
			output.rssi = rssi;
			output.signalLevel = wifiSignalLevelFromRssi(rssi);
		} else {
			output.signalLevel = 1;
		}
	}
	cyw43_arch_lwip_end();
#else
	output.supported = false;
#endif
	return output;
}

int runWifiConnectOnPico(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board) {
	if (!board.wireless.wifi) {
		context.writeLine("wifi-connect: Wi-Fi is not declared by this board profile");
		return 1;
	}

#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	auto const ssidText = arguments.positional("ssid");
	auto const passwordText = arguments.positionalOr("password", "");
	std::uint32_t timeoutMs = wifiConnectDefaultTimeoutMs;
	std::array<WifiAuthAttempt, 3> authAttempts{};
	std::size_t authAttemptCount = 0;

	if (ssidText.empty()) {
		context.writeErrorLine("wifi-connect: missing SSID");
		return 2;
	}

	if (!parseOptionalUnsignedArgument(arguments, "timeout", timeoutMs)
	    || timeoutMs == 0
	    || timeoutMs > wifiConnectMaxTimeoutMs) {
		context.writeErrorLine("wifi-connect: invalid --timeout");
		return 2;
	}

	auto const authText = arguments.valueOr("auth", "auto");
	if (authText.empty() || authText == "auto") {
		fillAutoWifiAuthAttempts(authAttempts, authAttemptCount);
	} else {
		WifiAuthAttempt fixedAuth{};
		if (!parseWifiAuth(authText, fixedAuth)) {
			context.writeErrorLine("wifi-connect: invalid --auth, expected auto, open, wpa, wpa2 or mixed");
			return 2;
		}
		authAttempts[0] = fixedAuth;
		authAttemptCount = 1;
	}

	if (authAttemptCount == 0) {
		context.writeErrorLine("wifi-connect: no authentication modes selected");
		return 2;
	}

	if (!(authAttemptCount == 1 && authAttempts[0].mode == CYW43_AUTH_OPEN) && passwordText.size() < 8U) {
		context.writeErrorLine("wifi-connect: password must contain at least 8 characters for WPA/WPA2");
		return 2;
	}

	char ssid[33]{};
	char password[wifiMaxPasswordLength + 1]{};
	if (!copyStringToBuffer(ssidText, ssid, sizeof(ssid))) {
		context.writeErrorLine("wifi-connect: SSID is too long");
		return 2;
	}
	if (!copyStringToBuffer(passwordText, password, sizeof(password))) {
		context.writeErrorLine("wifi-connect: password is too long");
		return 2;
	}
	if (!ensureCyw43Initialized(context, "wifi-connect")) {
		return 1;
	}

	ensureWifiStaModeEnabled();
	context.write("wifi-connect: connecting to ");
	context.write(ssid);
	context.write(" auth=");
	if (authAttemptCount > 1) {
		context.write("auto");
	} else {
		context.write(authAttempts[0].name);
	}
	context.writeLine("...");
	context.writeLine("...");
	for (std::size_t index = 0; index < authAttemptCount; ++index) {
		auto const& attempt = authAttempts[index];
		if (authAttemptCount > 1) {
			context.write("wifi-connect: trying auth=");
			context.write(attempt.name);
			context.writeLine("");
		}
		char const* passwordArgument = attempt.mode == CYW43_AUTH_OPEN ? nullptr : password;
		int result = 0;
		if (connectWifiWithStatusTrace(context, ssid, passwordArgument, attempt, timeoutMs, result)) {
			context.write("wifi-connect: connected auth=");
			context.write(attempt.name);
			context.writeLine("");
			return 0;
		}
		writeWifiConnectFailure(context, attempt.name, result);
		if (authAttemptCount == 1) {
			return 1;
		}
		sleepMs(context, 250);
	}
	context.writeErrorLine("wifi-connect: all authentication attempts failed");
	return 1;
#else
	(void) arguments;
	context.writeLine("wifi-connect: CYW43 LwIP support is unavailable in this build");
	return 1;
#endif
}

bool queryNetworkTimeOnPico(
    lexxos::os::AppContext& context,
    std::string_view server,
    std::uint32_t timeoutMs,
    lexxos::devices::rtc::DateTime& output,
    std::uint32_t& unixSeconds) {
#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	if (server.empty()) {
		context.writeErrorLine("rtc-sync: empty NTP server name");
		return false;
	}
	if (timeoutMs == 0 || timeoutMs > wifiMaxTimeoutMs) {
		context.writeErrorLine("rtc-sync: invalid timeout");
		return false;
	}
	char serverName[ntpMaxServerNameLength + 1]{};
	if (!copyStringToBuffer(server, serverName, sizeof(serverName))) {
		context.writeErrorLine("rtc-sync: NTP server name is too long");
		return false;
	}
	if (!ensureCyw43Initialized(context, "rtc-sync")) {
		return false;
	}
	ensureWifiStaModeEnabled();
	if (!wifiLinkIsUp()) {
		context.writeErrorLine("rtc-sync: Wi-Fi link is down; run wifi-connect first");
		return false;
	}

	auto const startMs = uptimeMs(context);
	context.write("rtc-sync: resolving ");
	context.write(serverName);
	context.writeLine("");
	ip_addr_t address{};
	if (!resolveNtpServer(context, serverName, startMs, timeoutMs, address)) {
		return false;
	}

	NtpQueryState ntpState{};
	context.writeLine("rtc-sync: sending NTP request");
	if (!startNtpRequest(context, address, ntpState)) {
		return false;
	}
	bool const received = waitForNtp(context, ntpState, startMs, timeoutMs);
	closeNtpRequest(ntpState);
	if (!received) {
		context.writeErrorLine("rtc-sync: NTP timeout or invalid response");
		return false;
	}
	if (!decodeNtpTime(ntpState, output, unixSeconds)) {
		context.writeErrorLine("rtc-sync: failed to decode NTP response");
		return false;
	}
	return true;
#else
	(void) context;
	(void) server;
	(void) timeoutMs;
	(void) output;
	(void) unixSeconds;
	context.writeLine("rtc-sync: CYW43 LwIP support is unavailable in this build");
	return false;
#endif
}

int runWifiStatusOnPico(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board) {
	(void) arguments;
	if (!board.wireless.wifi) {
		context.writeLine("wifi-status: Wi-Fi is not declared by this board profile");
		return 1;
	}

#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	if (!ensureCyw43Initialized(context, "wifi-status")) {
		return 1;
	}
	ensureWifiStaModeEnabled();

	WifiStatusSnapshot snapshot{};
	readWifiStatusSnapshot(snapshot);

	context.write("cyw43: initialized=");
	context.write(snapshot.cyw43Initialized ? "yes" : "no");
	context.write(" init-result=");
	writeSigned(context, snapshot.cyw43InitResult);
	context.writeLine("");
	context.write("sta-mode: enabled=");
	context.writeLine(snapshot.staModeEnabled ? "yes" : "no");
	context.write("link: ");
	context.write(wifiLinkStatusName(snapshot.linkStatus));
	context.write(" (");
	writeSigned(context, snapshot.linkStatus);
	context.writeLine(")");

	context.write("netif: up=");
	context.write(snapshot.netifUp ? "yes" : "no");
	context.write(" link=");
	context.write(snapshot.netifLinkUp ? "yes" : "no");
	context.write(" flags=0x");
	writeHexByte(context, snapshot.netifFlags);
	context.writeLine("");
	context.write("mac: ");
	writeMacAddress(context, snapshot);
	context.writeLine("");
	context.write("ip: ");
	writeIp4Address(context, snapshot.ip);
	context.writeLine("");
	context.write("netmask: ");
	writeIp4Address(context, snapshot.netmask);
	context.writeLine("");
	context.write("gateway: ");
	writeIp4Address(context, snapshot.gateway);
	context.writeLine("");
	context.write("dns0: ");
	writeIpAddress(context, snapshot.dns0);
	context.writeLine("");
	context.write("dns1: ");
	writeIpAddress(context, snapshot.dns1);
	context.writeLine("");
	return 0;
#else
	context.writeLine("wifi-status: CYW43 LwIP support is unavailable in this build");
	return 1;
#endif
}

int runPingOnPico(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board) {
	if (!board.wireless.wifi) {
		context.writeLine("ping: Wi-Fi is not declared by this board profile");
		return 1;
	}

#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	auto const targetText = arguments.positional("target");
	if (targetText.empty()) {
		context.writeErrorLine("ping: missing target host or IP");
		return 2;
	}
	std::uint32_t timeoutMs = pingDefaultTimeoutMs;
	if (!parseOptionalUnsignedArgument(arguments, "timeout", timeoutMs) || timeoutMs == 0 || timeoutMs > pingMaxTimeoutMs) {
		context.writeErrorLine("ping: invalid --timeout");
		return 2;
	}
	std::uint32_t count = pingDefaultCount;
	if (!parseOptionalUnsignedArgument(arguments, "count", count) || count == 0 || count > pingMaxCount) {
		context.writeErrorLine("ping: invalid --count");
		return 2;
	}

	char target[ntpMaxServerNameLength + 1]{};
	if (!copyStringToBuffer(targetText, target, sizeof(target))) {
		context.writeErrorLine("ping: target name is too long");
		return 2;
	}

	if (!ensureCyw43Initialized(context, "ping")) {
		return 1;
	}
	ensureWifiStaModeEnabled();
	int const linkStatus = readWifiLinkStatus();
	if (linkStatus != CYW43_LINK_UP) {
		context.writeError("ping: Wi-Fi link is not up: ");
		context.writeError(wifiLinkStatusName(linkStatus));
		context.writeError(" (");
		writeSigned(context, linkStatus);
		context.writeErrorLine(")");
		return 1;
	}

	ip_addr_t address{};
	auto const resolveStartMs = uptimeMs(context);
	if (!resolveNetworkHost(context, "ping", target, resolveStartMs, timeoutMs, address)) {
		return 1;
	}

	context.write("PING ");
	context.write(target);
	context.write(" (");
	writeIpAddress(context, address);
	context.write("): ");
	writeUnsigned(context, static_cast<std::uint32_t>(pingPayloadBytes));
	context.writeLine(" data bytes");

	std::uint32_t transmitted = 0;
	std::uint32_t received = 0;
	bool cancelled = false;
	for (std::uint32_t index = 0; index < count; ++index) {
		if (context.pollCancellationRequest()) {
			cancelled = true;
			break;
		}

		PingQueryState state{};
		state.address = address;
		state.identifier = pingIdentifier;
		state.sequence = ++g_pingSequence;
		auto const sentMs = uptimeMs(context);
		if (!startPingRequest(context, state)) {
			closePingRequest(state);
			return received > 0 ? 0 : 1;
		}
		++transmitted;
		auto const waitResult = waitForPing(context, state, sentMs, timeoutMs);
		closePingRequest(state);
		if (waitResult == PingWaitResult::Cancelled) {
			cancelled = true;
			context.writeLine("ping: cancelled");
			break;
		}

		if (waitResult == PingWaitResult::Reply) {
			++received;
			writeUnsigned(context, state.responseLength);
			context.write(" bytes from ");
			writeIpAddress(context, address);
			context.write(": icmp_seq=");
			writeUnsigned(context, state.sequence);
			context.write(" time=");
			writeUnsigned(context, elapsedMs(sentMs, uptimeMs(context)));
			context.writeLine(" ms");
		} else {
			context.write("request timeout for icmp_seq=");
			writeUnsigned(context, state.sequence);
			context.writeLine("");
		}
		if (index + 1U < count) {
			if (!sleepPingInterval(context, 1000)) {
				cancelled = true;
				break;
			}
		}
	}

	context.writeLine("--- ping statistics ---");
	writeUnsigned(context, transmitted);
	context.write(" packets transmitted, ");
	writeUnsigned(context, received);
	context.writeLine(" received");
	return cancelled ? 130 : (received > 0 ? 0 : 1);
#else
	context.writeLine("ping: CYW43 LwIP support is unavailable in this build");
	return 1;
#endif
}

int runWifiScanOnPico(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board) {
	if (!board.wireless.wifi) {
		context.writeLine("wifi-scan: Wi-Fi is not declared by this board profile");
		return 1;
	}

#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE)
	std::uint32_t timeoutMs = wifiDefaultTimeoutMs;
	std::uint32_t limit = wifiDefaultLimit;
	bool const raw = arguments.has("raw");
	bool const showBssid = raw || arguments.has("bssid");
	if (!parseOptionalUnsignedArgument(arguments, "timeout", timeoutMs) || timeoutMs > wifiMaxTimeoutMs) {
		context.writeErrorLine("wifi-scan: invalid --timeout");
		return 2;
	}
	if (!parseOptionalUnsignedArgument(arguments, "limit", limit) || limit == 0 || limit > wifiMaxResults) {
		context.writeErrorLine("wifi-scan: invalid --limit");
		return 2;
	}

	if (!ensureCyw43Initialized(context, "wifi-scan")) {
		return 1;
	}

	if (wifiScanActive()) {
		context.writeLine("wifi-scan: previous scan is still active");
		return 1;
	}

	resetWifiScanState(g_wifiScanState, limit, raw, showBssid);
	cyw43_arch_lwip_begin();
	ensureWifiStaModeEnabled();
	cyw43_wifi_scan_options_t options{};
	int const scanResult = cyw43_wifi_scan(&cyw43_state, &options, &g_wifiScanState, wifiScanCallback);
	cyw43_arch_lwip_end();
	if (scanResult != 0) {
		g_wifiScanState.armed = false;
		context.writeError("wifi-scan: scan start failed: ");
		writeSigned(context, scanResult);
		context.writeErrorLine("");
		return 1;
	}

	context.writeLine("wifi-scan: scanning...");
	auto const startMs = uptimeMs(context);
	while (wifiScanActive()) {
		if (timeoutMs > 0 && elapsedMs(startMs, uptimeMs(context)) >= timeoutMs) {
			context.writeLine("wifi-scan: timeout while scan is still active");
			context.writeLine("wifi-scan: wait a moment before starting another scan");
			return 1;
		}
		cyw43_arch_poll();
		sleepMs(context, 100);
	}

	g_wifiScanState.armed = false;
	writeWifiScanResults(context, g_wifiScanState);
	return 0;
#else
	(void) arguments;
	context.writeLine("wifi-scan: CYW43 support is unavailable in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::network_programs::pico_detail
