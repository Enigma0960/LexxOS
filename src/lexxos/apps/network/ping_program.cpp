#include <lexxos/apps/network/ping_program.h>

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
	#include <lwip/inet_chksum.h>
	#include <lwip/netdb.h>
	#include <lwip/prot/icmp.h>
	#include <lwip/sockets.h>
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	#include <lexxos/apps/network/detail/pico_network_support.h>
#endif

namespace lexxos::apps::network_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentValueType;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	constexpr std::uint32_t pingDefaultCount = 4;
	constexpr std::uint32_t pingMaxCount = 10;
	constexpr std::uint32_t pingDefaultTimeoutMs = 5000;
	constexpr std::uint32_t pingMaxTimeoutMs = 60000;
	constexpr std::size_t pingPayloadBytes = 32;
	constexpr std::size_t maxPingTargetLength = 127;

	constexpr lexxos::program::ProgramArgumentSpec pingArguments[] = {
	    {
         .name = "count",
         .shortName = 'c',
         .valueName = "n",
         .help = "number of ICMP echo requests, 1..10",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "4",
	     },
	    {
         .name = "timeout",
         .shortName = 't',
         .valueName = "ms",
         .help = "timeout per request and DNS resolve",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "5000",
	     },
	    {
         .name = "target",
         .valueName = "host-or-ip",
         .help = "IPv4 address or DNS host name",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = true,
	     },
	};

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
	bool parsePingArguments(lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::uint32_t& count,
	    std::uint32_t& timeoutMs,
	    char (&target)[maxPingTargetLength + 1U]) {
		auto const targetText = arguments.positional("target");
		if (targetText.empty()) {
			context.writeErrorLine("ping: missing target host or IP");
			return false;
		}
		if (!copyToBuffer(targetText, target, sizeof(target))) {
			context.writeErrorLine("ping: target name is too long");
			return false;
		}

		if (!parseUnsigned32(arguments.valueOr("count", "4"), count) || count == 0U || count > pingMaxCount) {
			context.writeErrorLine("ping: invalid --count");
			return false;
		}
		if (!parseUnsigned32(arguments.valueOr("timeout", "5000"), timeoutMs)
		    || timeoutMs == 0U
		    || timeoutMs > pingMaxTimeoutMs) {
			context.writeErrorLine("ping: invalid --timeout");
			return false;
		}
		return true;
	}

	void writeSocketError(lexxos::os::AppContext& context, std::string_view prefix, int error) {
		context.writeError(prefix);
		context.writeError(": ");
		context.writeErrorLine(std::strerror(error));
	}

	void writeIpv4(lexxos::os::AppContext& context, in_addr const& address) {
		char text[INET_ADDRSTRLEN]{};
		if (inet_ntop(AF_INET, &address, text, sizeof(text)) == nullptr) {
			context.write("?");
			return;
		}
		context.write(text);
	}

	bool resolveTarget(lexxos::os::AppContext& context, char const* target, sockaddr_in& output) {
		in_addr direct{};
		if (inet_pton(AF_INET, target, &direct) == 1) {
			output = {};
			output.sin_family = AF_INET;
			output.sin_addr = direct;
			return true;
		}

		context.write("ping: resolving ");
		context.writeLine(target);
		addrinfo hints{};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_RAW;
		hints.ai_protocol = IPPROTO_ICMP;

		addrinfo* results = nullptr;
		int const result = getaddrinfo(target, nullptr, &hints, &results);
		if (result != 0 || results == nullptr) {
			char errorCode[16]{};
			std::snprintf(errorCode, sizeof(errorCode), "%d", result);
			context.writeError("ping: DNS resolve failed: code ");
			context.writeErrorLine(errorCode);
			return false;
		}

		output = *reinterpret_cast<sockaddr_in const*>(results->ai_addr);
		freeaddrinfo(results);
		return true;
	}

	void fillPingPayload(std::uint8_t* payload, std::size_t size, std::uint16_t sequence) {
		for (std::size_t index = 0; index < size; ++index) {
			payload[index] = static_cast<std::uint8_t>('A' + ((index + sequence) % 26U));
		}
	}

	bool sendPingRequest(int socketHandle,
	    sockaddr_in const& destination,
	    std::uint16_t identifier,
	    std::uint16_t sequence) {
		struct EchoPacket {
			icmp_echo_hdr header{};
			std::uint8_t payload[pingPayloadBytes]{};
		};

		EchoPacket packet{};
		ICMPH_TYPE_SET(&packet.header, ICMP_ECHO);
		ICMPH_CODE_SET(&packet.header, 0);
		packet.header.id = htons(identifier);
		packet.header.seqno = htons(sequence);
		fillPingPayload(packet.payload, sizeof(packet.payload), sequence);
		packet.header.chksum = 0;
		packet.header.chksum = inet_chksum(&packet, sizeof(packet));

		auto const* address = reinterpret_cast<sockaddr const*>(&destination);
		return sendto(socketHandle, &packet, sizeof(packet), 0, address, sizeof(destination)) == static_cast<int>(sizeof(packet));
	}

	bool decodePingReply(std::uint8_t const* buffer,
	    std::size_t length,
	    std::uint16_t identifier,
	    std::uint16_t sequence,
	    std::size_t& payloadLength) {
		if (buffer == nullptr || length < sizeof(icmp_echo_hdr)) {
			return false;
		}

		std::size_t offset = 0;
		if (length >= 20U && (buffer[0] >> 4U) == 4U) {
			std::size_t const headerLength = static_cast<std::size_t>(buffer[0] & 0x0FU) * 4U;
			if (headerLength >= 20U
			    && length >= headerLength + sizeof(icmp_echo_hdr)
			    && buffer[9] == IPPROTO_ICMP) {
				offset = headerLength;
			}
		}

		if (length < offset + sizeof(icmp_echo_hdr)) {
			return false;
		}

		icmp_echo_hdr header{};
		std::memcpy(&header, buffer + offset, sizeof(header));
		if (ICMPH_TYPE(&header) != ICMP_ER
		    || ntohs(header.id) != identifier
		    || ntohs(header.seqno) != sequence) {
			return false;
		}

		payloadLength = length - offset - sizeof(icmp_echo_hdr);
		return true;
	}

	enum class PingReceiveResult : std::uint8_t {
		Reply,
		Timeout,
		Error,
	};

	PingReceiveResult waitForPingReply(lexxos::os::AppContext& context,
	    int socketHandle,
	    std::uint16_t identifier,
	    std::uint16_t sequence,
	    std::uint32_t sentMs,
	    std::uint32_t timeoutMs,
	    std::size_t& payloadLength,
	    std::uint32_t& elapsedMs) {
		while (uptimeMs(context) - sentMs < timeoutMs) {
			std::uint8_t buffer[128]{};
			sockaddr_in source{};
			socklen_t sourceLength = sizeof(source);
			auto const received = recvfrom(socketHandle,
			    buffer,
			    sizeof(buffer),
			    0,
			    reinterpret_cast<sockaddr*>(&source),
			    &sourceLength);
			if (received < 0) {
				int const error = errno;
				if (error == EWOULDBLOCK || error == EAGAIN || error == ETIMEDOUT) {
					return PingReceiveResult::Timeout;
				}
				if (error == EINTR) {
					continue;
				}
				writeSocketError(context, "ping: receive failed", error);
				return PingReceiveResult::Error;
			}

			if (decodePingReply(buffer,
			        static_cast<std::size_t>(received),
			        identifier,
			        sequence,
			        payloadLength)) {
				elapsedMs = uptimeMs(context) - sentMs;
				return PingReceiveResult::Reply;
			}
		}
		return PingReceiveResult::Timeout;
	}

	int runPingOnEspIdf(lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments) {
		std::uint32_t count = pingDefaultCount;
		std::uint32_t timeoutMs = pingDefaultTimeoutMs;
		char target[maxPingTargetLength + 1U]{};
		if (!parsePingArguments(context, arguments, count, timeoutMs, target)) {
			return 2;
		}

		sockaddr_in destination{};
		if (!resolveTarget(context, target, destination)) {
			return 1;
		}

		int const socketHandle = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (socketHandle < 0) {
			writeSocketError(context, "ping: socket open failed", errno);
			return 1;
		}

		timeval timeout{};
		timeout.tv_sec = static_cast<time_t>(timeoutMs / 1000U);
		timeout.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000U) * 1000U);
		if (setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
			writeSocketError(context, "ping: failed to set socket timeout", errno);
			close(socketHandle);
			return 1;
		}

		context.write("PING ");
		context.write(target);
		context.write(" (");
		writeIpv4(context, destination.sin_addr);
		context.write("): ");
		writeUnsigned(context, static_cast<std::uint32_t>(pingPayloadBytes));
		context.writeLine(" data bytes");

		constexpr std::uint16_t identifier = 0x4C58U;
		std::uint32_t transmitted = 0;
		std::uint32_t received = 0;
		for (std::uint32_t index = 0; index < count; ++index) {
			if (context.pollCancellationRequest()) {
				break;
			}

			std::uint16_t const sequence = static_cast<std::uint16_t>(index + 1U);
			std::uint32_t const sentMs = uptimeMs(context);
			if (!sendPingRequest(socketHandle, destination, identifier, sequence)) {
				writeSocketError(context, "ping: send failed", errno);
				close(socketHandle);
				return received > 0U ? 0 : 1;
			}
			++transmitted;

			std::size_t payloadLength = 0;
			std::uint32_t elapsedMs = 0;
			auto const result = waitForPingReply(context,
			    socketHandle,
			    identifier,
			    sequence,
			    sentMs,
			    timeoutMs,
			    payloadLength,
			    elapsedMs);
			if (result == PingReceiveResult::Error) {
				close(socketHandle);
				return received > 0U ? 0 : 1;
			}
			if (result == PingReceiveResult::Timeout) {
				context.write("Request timeout for icmp_seq ");
				writeUnsigned(context, sequence);
				context.writeLine("");
			} else {
				++received;
				writeUnsigned(context, static_cast<std::uint32_t>(payloadLength));
				context.write(" bytes from ");
				writeIpv4(context, destination.sin_addr);
				context.write(": icmp_seq=");
				writeUnsigned(context, sequence);
				context.write(" time=");
				writeUnsigned(context, elapsedMs);
				context.writeLine(" ms");
			}

			if (index + 1U < count) {
				sleepMs(context, 1000);
			}
		}

		close(socketHandle);
		context.writeLine("--- ping statistics ---");
		writeUnsigned(context, transmitted);
		context.write(" packets transmitted, ");
		writeUnsigned(context, received);
		context.writeLine(" received");
		return received > 0U ? 0 : 1;
	}
#endif

} // namespace

lexxos::program::ProgramDescriptor PingProgram::descriptor() {
	return {
	    .name = "ping",
	    .help = "check IP connectivity using ICMP echo",
	    .description = "Sends ICMP echo requests through the active board network interface.",
	    .usage = "ping [--count <n>] [--timeout <ms>] <host-or-ip>",
	    .arguments = argumentList(pingArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 6144,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int PingProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	return runPingOnEspIdf(context, arguments);
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	return lexxos::apps::network_programs::pico_detail::runPingOnPico(context, arguments, activeBoard(context));
#else
	context.writeErrorLine("ping: network ICMP support is unavailable in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::network_programs
