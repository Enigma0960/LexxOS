#include <lexxos/apps/network/network_time.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <cerrno>
	#include <cstdio>
	#include <sys/time.h>

	#include <lwip/inet.h>
	#include <lwip/netdb.h>
	#include <lwip/sockets.h>
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	#include <lexxos/apps/network/detail/pico_network_support.h>
#endif

namespace lexxos::apps::network_programs {
namespace {

	using lexxos::devices::rtc::DateTime;

	constexpr std::uint32_t ntpUnixEpochDeltaSeconds = 2208988800UL;
	constexpr std::uint32_t ntpPort = 123;
	constexpr std::size_t ntpPacketSize = 48;
	constexpr std::size_t maxNtpServerNameLength = 127;

	bool copyToBuffer(std::string_view text, char* output, std::size_t outputSize) {
		if (output == nullptr || outputSize == 0U || text.size() >= outputSize) {
			return false;
		}
		for (std::size_t index = 0; index < text.size(); ++index) {
			output[index] = text[index];
		}
		output[text.size()] = '\0';
		return true;
	}

#if defined(LEXXOS_PLATFORM_ESPIDF)
	void writeSocketError(lexxos::os::AppContext& context, std::string_view prefix, int error) {
		context.writeError(prefix);
		context.writeError(": ");
		context.writeErrorLine(std::strerror(error));
	}

	bool resolveNtpServer(lexxos::os::AppContext& context, char const* server, sockaddr_in& output) {
		context.write("rtc-sync: resolving ");
		context.writeLine(server);
		addrinfo hints{};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;

		addrinfo* results = nullptr;
		int const result = getaddrinfo(server, "123", &hints, &results);
		if (result != 0 || results == nullptr) {
			char errorCode[16]{};
			std::snprintf(errorCode, sizeof(errorCode), "%d", result);
			context.writeError("rtc-sync: DNS resolve failed: code ");
			context.writeErrorLine(errorCode);
			return false;
		}

		output = *reinterpret_cast<sockaddr_in const*>(results->ai_addr);
		output.sin_port = htons(static_cast<std::uint16_t>(ntpPort));
		freeaddrinfo(results);
		return true;
	}

	bool decodeNtpResponse(std::uint8_t const (&packet)[ntpPacketSize], DateTime& output, std::uint32_t& unixSeconds) {
		std::uint32_t const ntpSeconds = (static_cast<std::uint32_t>(packet[40]) << 24U)
		    | (static_cast<std::uint32_t>(packet[41]) << 16U)
		    | (static_cast<std::uint32_t>(packet[42]) << 8U)
		    | static_cast<std::uint32_t>(packet[43]);
		if (ntpSeconds < ntpUnixEpochDeltaSeconds) {
			return false;
		}

		unixSeconds = ntpSeconds - ntpUnixEpochDeltaSeconds;
		return lexxos::devices::rtc::unixSecondsToDateTime(unixSeconds, output);
	}

	bool queryNetworkTimeOnEspIdf(lexxos::os::AppContext& context,
	    std::string_view server,
	    std::uint32_t timeoutMs,
	    DateTime& output,
	    std::uint32_t& unixSeconds) {
		if (server.empty()) {
			context.writeErrorLine("rtc-sync: empty NTP server name");
			return false;
		}
		if (timeoutMs == 0U || timeoutMs > 60000U) {
			context.writeErrorLine("rtc-sync: invalid timeout");
			return false;
		}

		char serverName[maxNtpServerNameLength + 1U]{};
		if (!copyToBuffer(server, serverName, sizeof(serverName))) {
			context.writeErrorLine("rtc-sync: NTP server name is too long");
			return false;
		}

		sockaddr_in destination{};
		if (!resolveNtpServer(context, serverName, destination)) {
			return false;
		}

		int const socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (socketHandle < 0) {
			writeSocketError(context, "rtc-sync: UDP socket open failed", errno);
			return false;
		}

		timeval timeout{};
		timeout.tv_sec = static_cast<time_t>(timeoutMs / 1000U);
		timeout.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000U) * 1000U);
		if (setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
			writeSocketError(context, "rtc-sync: failed to set socket timeout", errno);
			close(socketHandle);
			return false;
		}

		std::uint8_t packet[ntpPacketSize]{};
		packet[0] = 0x1BU;
		context.writeLine("rtc-sync: sending NTP request");
		auto const sent = sendto(socketHandle,
		    packet,
		    sizeof(packet),
		    0,
		    reinterpret_cast<sockaddr const*>(&destination),
		    sizeof(destination));
		if (sent != static_cast<int>(sizeof(packet))) {
			writeSocketError(context, "rtc-sync: UDP send failed", errno);
			close(socketHandle);
			return false;
		}

		sockaddr_in source{};
		socklen_t sourceLength = sizeof(source);
		auto const received = recvfrom(socketHandle,
		    packet,
		    sizeof(packet),
		    0,
		    reinterpret_cast<sockaddr*>(&source),
		    &sourceLength);
		if (received < static_cast<int>(sizeof(packet))) {
			if (received < 0) {
				int const error = errno;
				if (error == EWOULDBLOCK || error == EAGAIN || error == ETIMEDOUT) {
					context.writeErrorLine("rtc-sync: NTP timeout or invalid response");
				} else {
					writeSocketError(context, "rtc-sync: UDP receive failed", error);
				}
			} else {
				context.writeErrorLine("rtc-sync: NTP timeout or invalid response");
			}
			close(socketHandle);
			return false;
		}

		close(socketHandle);
		if (!decodeNtpResponse(packet, output, unixSeconds)) {
			context.writeErrorLine("rtc-sync: failed to decode NTP response");
			return false;
		}
		return true;
	}
#endif

} // namespace

bool queryNetworkTime(lexxos::os::AppContext& context,
    std::string_view server,
    std::uint32_t timeoutMs,
    DateTime& output,
    std::uint32_t& unixSeconds) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	return queryNetworkTimeOnEspIdf(context, server, timeoutMs, output, unixSeconds);
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	return lexxos::apps::network_programs::pico_detail::queryNetworkTimeOnPico(
	    context,
	    server,
	    timeoutMs,
	    output,
	    unixSeconds);
#else
	(void) server;
	(void) timeoutMs;
	(void) output;
	(void) unixSeconds;
	context.writeErrorLine("rtc-sync: network NTP support is unavailable in this build");
	return false;
#endif
}

} // namespace lexxos::apps::network_programs
