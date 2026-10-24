#include <lexxos/apps/fs/ymodem_transfer_programs.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include <lexxos/app/console_input.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	constexpr std::uint8_t soh = 0x01;
	constexpr std::uint8_t stx = 0x02;
	constexpr std::uint8_t eot = 0x04;
	constexpr std::uint8_t ack = 0x06;
	constexpr std::uint8_t nak = 0x15;
	constexpr std::uint8_t can = 0x18;
	constexpr std::uint8_t crcRequest = static_cast<std::uint8_t>('C');
	constexpr std::uint8_t cpmEof = 0x1A;

	constexpr std::size_t shortPacketBytes = 128;
	constexpr std::size_t longPacketBytes = 1024;
	constexpr std::uint8_t maxRetries = 10;
	constexpr std::uint32_t startupTimeoutMs = 2000;
	constexpr std::uint32_t packetTimeoutMs = 10000;
	constexpr std::uint32_t byteTimeoutMs = 1000;
	constexpr std::uint32_t receiverWaitMs = 60000;

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using PacketData = std::array<std::uint8_t, longPacketBytes>;

	enum class TransferStatus : std::uint8_t {
		Ok,
		Timeout,
		Cancelled,
		RemoteCancelled,
		ProtocolError,
		CrcError,
		ConsoleUnavailable,
		InvalidPath,
		OpenFailed,
		ReadFailed,
		WriteFailed,
	};

	struct TransferResult {
		TransferStatus status{TransferStatus::Ok};
		std::uint64_t bytes{0};
		lexxos::vfs::OpenStatus openStatus{lexxos::vfs::OpenStatus::Ok};
		lexxos::vfs::ReadStatus readStatus{lexxos::vfs::ReadStatus::Ok};
		lexxos::vfs::WriteStatus writeStatus{lexxos::vfs::WriteStatus::Ok};
	};

	struct YmodemPacket {
		bool eot{false};
		bool cancel{false};
		std::uint8_t sequence{0};
		std::size_t dataSize{0};
		PacketData data{};
	};

	struct YmodemMetadata {
		bool empty{true};
		bool hasSize{false};
		std::string_view fileName{};
		std::uint64_t sizeBytes{0};
	};

	class RawConsoleModeGuard final {
	public:
		RawConsoleModeGuard()
		    : _previous(lexxos::app::console_input::mode()) {
			lexxos::app::console_input::setMode(lexxos::app::console_input::Mode::RawProgram);
		}

		RawConsoleModeGuard(RawConsoleModeGuard const&) = delete;
		RawConsoleModeGuard& operator=(RawConsoleModeGuard const&) = delete;

		~RawConsoleModeGuard() {
			lexxos::app::console_input::setMode(_previous);
		}

	private:
		lexxos::app::console_input::Mode _previous;
	};

	constexpr ProgramArgumentSpec receiveArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "optional VFS destination path; when omitted, the YMODEM file name is used",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Path,
         .required = false,
	     },
	};

	constexpr ProgramArgumentSpec sendArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "VFS source file path",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Path,
         .required = true,
	     },
	    {
         .name = "name",
         .valueName = "name",
         .help = "optional transmitted file name",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	std::string_view toString(TransferStatus status) {
		switch (status) {
			case TransferStatus::Ok:
				return "ok";
			case TransferStatus::Timeout:
				return "timeout";
			case TransferStatus::Cancelled:
				return "cancelled";
			case TransferStatus::RemoteCancelled:
				return "remote cancelled";
			case TransferStatus::ProtocolError:
				return "protocol error";
			case TransferStatus::CrcError:
				return "crc error";
			case TransferStatus::ConsoleUnavailable:
				return "console is unavailable";
			case TransferStatus::InvalidPath:
				return "invalid path";
			case TransferStatus::OpenFailed:
				return "open failed";
			case TransferStatus::ReadFailed:
				return "read failed";
			case TransferStatus::WriteFailed:
				return "write failed";
		}
		return "unknown";
	}

	bool elapsed(std::uint32_t startedMs, std::uint32_t nowMs, std::uint32_t timeoutMs) {
		return static_cast<std::uint32_t>(nowMs - startedMs) >= timeoutMs;
	}

	bool writeRaw(lexxos::os::AppContext& context, std::uint8_t const* data, std::size_t size) {
		auto const& console = context.platform().console;
		if (console.write == nullptr) {
			return false;
		}

		console.write(std::string_view{reinterpret_cast<char const*>(data), size});
		return true;
	}

	bool writeByte(lexxos::os::AppContext& context, std::uint8_t value) {
		return writeRaw(context, &value, 1);
	}

	void sendCancel(lexxos::os::AppContext& context) {
		(void) writeByte(context, can);
		(void) writeByte(context, can);
	}

	TransferStatus readByte(
	    lexxos::os::AppContext& context,
	    std::uint8_t& value,
	    std::uint32_t timeoutMs) {
		auto const started = lexxos::apps::uptimeMs(context);
		while (true) {
			char raw{};
			if (lexxos::app::console_input::readChar(context.platform(), raw)) {
				value = static_cast<std::uint8_t>(raw);
				return TransferStatus::Ok;
			}

			if (context.cancellationRequested()) {
				return TransferStatus::Cancelled;
			}

			if (elapsed(started, lexxos::apps::uptimeMs(context), timeoutMs)) {
				return TransferStatus::Timeout;
			}
			lexxos::apps::sleepMs(context, 2);
		}
	}

	TransferStatus readRequired(
	    lexxos::os::AppContext& context,
	    std::uint8_t* output,
	    std::size_t size,
	    std::uint32_t timeoutMs = byteTimeoutMs) {
		for (std::size_t index = 0; index < size; ++index) {
			auto const status = readByte(context, output[index], timeoutMs);
			if (status != TransferStatus::Ok) {
				return status;
			}
		}
		return TransferStatus::Ok;
	}

	std::uint16_t crc16Xmodem(std::uint8_t const* data, std::size_t size) {
		std::uint16_t crc = 0;
		for (std::size_t index = 0; index < size; ++index) {
			crc ^= static_cast<std::uint16_t>(data[index]) << 8U;
			for (std::uint8_t bit = 0; bit < 8; ++bit) {
				if ((crc & 0x8000U) != 0) {
					crc = static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U);
				} else {
					crc = static_cast<std::uint16_t>(crc << 1U);
				}
			}
		}
		return crc;
	}

	TransferStatus receivePacket(
	    lexxos::os::AppContext& context,
	    YmodemPacket& packet,
	    std::uint32_t timeoutMs) {
		packet = {};

		std::uint8_t start{};
		auto status = readByte(context, start, timeoutMs);
		if (status != TransferStatus::Ok) {
			return status;
		}

		if (start == eot) {
			packet.eot = true;
			return TransferStatus::Ok;
		}
		if (start == can) {
			packet.cancel = true;
			return TransferStatus::RemoteCancelled;
		}
		if (start != soh && start != stx) {
			return TransferStatus::ProtocolError;
		}

		packet.dataSize = start == soh ? shortPacketBytes : longPacketBytes;

		std::uint8_t header[2]{};
		status = readRequired(context, header, sizeof(header));
		if (status != TransferStatus::Ok) {
			return status;
		}

		packet.sequence = header[0];
		if (static_cast<std::uint8_t>(header[0] + header[1]) != 0xFFU) {
			return TransferStatus::ProtocolError;
		}

		status = readRequired(context, packet.data.data(), packet.dataSize);
		if (status != TransferStatus::Ok) {
			return status;
		}

		std::uint8_t crcBytes[2]{};
		status = readRequired(context, crcBytes, sizeof(crcBytes));
		if (status != TransferStatus::Ok) {
			return status;
		}

		auto const receivedCrc =
		    static_cast<std::uint16_t>((static_cast<std::uint16_t>(crcBytes[0]) << 8U) | crcBytes[1]);
		if (receivedCrc != crc16Xmodem(packet.data.data(), packet.dataSize)) {
			return TransferStatus::CrcError;
		}

		return TransferStatus::Ok;
	}

	TransferStatus sendPacketOnce(
	    lexxos::os::AppContext& context,
	    std::uint8_t sequence,
	    std::uint8_t const* data,
	    std::size_t dataSize) {
		if (dataSize != shortPacketBytes && dataSize != longPacketBytes) {
			return TransferStatus::ProtocolError;
		}

		std::uint8_t const start = dataSize == longPacketBytes ? stx : soh;
		std::uint8_t header[3]{
		    start,
		    sequence,
		    static_cast<std::uint8_t>(0xFFU - sequence),
		};
		auto const crc = crc16Xmodem(data, dataSize);
		std::uint8_t crcBytes[2]{
		    static_cast<std::uint8_t>((crc >> 8U) & 0xFFU),
		    static_cast<std::uint8_t>(crc & 0xFFU),
		};

		if (!writeRaw(context, header, sizeof(header))
		    || !writeRaw(context, data, dataSize)
		    || !writeRaw(context, crcBytes, sizeof(crcBytes))) {
			return TransferStatus::ConsoleUnavailable;
		}
		return TransferStatus::Ok;
	}

	TransferStatus sendPacketWithRetry(
	    lexxos::os::AppContext& context,
	    std::uint8_t sequence,
	    std::uint8_t const* data,
	    std::size_t dataSize) {
		for (std::uint8_t attempt = 0; attempt < maxRetries; ++attempt) {
			auto status = sendPacketOnce(context, sequence, data, dataSize);
			if (status != TransferStatus::Ok) {
				return status;
			}

			std::uint8_t response{};
			status = readByte(context, response, packetTimeoutMs);
			if (status != TransferStatus::Ok) {
				continue;
			}
			if (response == ack) {
				return TransferStatus::Ok;
			}
			if (response == nak || response == crcRequest) {
				continue;
			}
			if (response == can) {
				return TransferStatus::RemoteCancelled;
			}
		}

		sendCancel(context);
		return TransferStatus::Timeout;
	}

	TransferStatus waitForReceiverRequest(lexxos::os::AppContext& context, std::uint32_t timeoutMs) {
		auto const started = lexxos::apps::uptimeMs(context);
		while (!elapsed(started, lexxos::apps::uptimeMs(context), timeoutMs)) {
			std::uint8_t value{};
			auto const status = readByte(context, value, 1000);
			if (status == TransferStatus::Timeout) {
				continue;
			}
			if (status != TransferStatus::Ok) {
				return status;
			}
			if (value == crcRequest) {
				return TransferStatus::Ok;
			}
			if (value == can) {
				return TransferStatus::RemoteCancelled;
			}
		}
		return TransferStatus::Timeout;
	}

	bool appendDecimal(char* output, std::size_t capacity, std::uint64_t value, std::size_t& position) {
		char digits[20]{};
		std::size_t digitCount = 0;
		do {
			digits[digitCount++] = static_cast<char>('0' + (value % 10ULL));
			value /= 10ULL;
		} while (value != 0 && digitCount < sizeof(digits));

		if (position + digitCount >= capacity) {
			return false;
		}

		while (digitCount != 0) {
			output[position++] = digits[--digitCount];
		}
		return true;
	}

	std::string_view basenameOf(std::string_view path) {
		auto const slash = path.find_last_of('/');
		if (slash == std::string_view::npos) {
			return path;
		}
		return path.substr(slash + 1U);
	}

	bool isSafeReceivedName(std::string_view name) {
		if (name.empty()) {
			return false;
		}
		for (char const value : name) {
			auto const byte = static_cast<std::uint8_t>(value);
			if (byte < 0x20U || value == '/' || value == '\\' || value == ':') {
				return false;
			}
		}
		return true;
	}

	bool parseDecimalField(std::string_view field, std::uint64_t& value) {
		if (field.empty()) {
			return false;
		}

		std::uint64_t parsed = 0;
		for (char const valueChar : field) {
			if (valueChar < '0' || valueChar > '9') {
				return false;
			}
			auto const digit = static_cast<std::uint64_t>(valueChar - '0');
			if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10ULL) {
				return false;
			}
			parsed = parsed * 10ULL + digit;
		}
		value = parsed;
		return true;
	}

	YmodemMetadata parseMetadata(YmodemPacket const& packet) {
		YmodemMetadata metadata{};
		if (packet.dataSize == 0 || packet.data[0] == 0) {
			return metadata;
		}

		auto const* text = reinterpret_cast<char const*>(packet.data.data());
		std::size_t nameLength = 0;
		while (nameLength < packet.dataSize && text[nameLength] != '\0') {
			++nameLength;
		}
		if (nameLength == packet.dataSize) {
			return metadata;
		}

		metadata.empty = false;
		metadata.fileName = std::string_view{text, nameLength};

		auto sizeStart = nameLength + 1U;
		while (sizeStart < packet.dataSize && text[sizeStart] == ' ') {
			++sizeStart;
		}
		auto sizeEnd = sizeStart;
		while (sizeEnd < packet.dataSize && text[sizeEnd] != '\0' && text[sizeEnd] != ' ') {
			++sizeEnd;
		}

		std::uint64_t sizeBytes = 0;
		if (parseDecimalField(std::string_view{text + sizeStart, sizeEnd - sizeStart}, sizeBytes)) {
			metadata.hasSize = true;
			metadata.sizeBytes = sizeBytes;
		}
		return metadata;
	}

	bool resolveTargetPath(
	    lexxos::os::AppContext& context,
	    std::string_view explicitPath,
	    std::string_view receivedName,
	    PathBuffer& output) {
		std::string_view selectedPath = explicitPath;
		if (selectedPath.empty()) {
			if (!isSafeReceivedName(receivedName)) {
				return false;
			}
			selectedPath = receivedName;
		}
		return lexxos::vfs::normalizePath(context.workingDirectory(), selectedPath, output.data(), output.size());
	}

	TransferResult receiveYmodem(lexxos::os::AppContext& context, std::string_view targetPath) {
		RawConsoleModeGuard rawMode;

		YmodemPacket packet{};
		YmodemMetadata metadata{};
		bool metadataReady = false;

		for (std::uint8_t attempt = 0; attempt < 30 && !metadataReady; ++attempt) {
			if (!writeByte(context, crcRequest)) {
				return {.status = TransferStatus::ConsoleUnavailable};
			}

			auto const status = receivePacket(context, packet, startupTimeoutMs);
			if (status == TransferStatus::Timeout) {
				continue;
			}
			if (status != TransferStatus::Ok) {
				sendCancel(context);
				return {.status = status};
			}
			if (packet.eot || packet.sequence != 0) {
				sendCancel(context);
				return {.status = TransferStatus::ProtocolError};
			}

			metadata = parseMetadata(packet);
			if (metadata.empty) {
				(void) writeByte(context, ack);
				return {};
			}
			metadataReady = true;
		}

		if (!metadataReady) {
			sendCancel(context);
			return {.status = TransferStatus::Timeout};
		}

		PathBuffer path{};
		if (!resolveTargetPath(context, targetPath, metadata.fileName, path)) {
			sendCancel(context);
			return {.status = TransferStatus::InvalidPath};
		}

		lexxos::vfs::FileHandle file;
		auto const openStatus = lexxos::vfs::open(path.data(), lexxos::vfs::OpenMode::CreateTruncateWrite, file);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			sendCancel(context);
			return {.status = TransferStatus::OpenFailed, .openStatus = openStatus};
		}

		if (!writeByte(context, ack) || !writeByte(context, crcRequest)) {
			return {.status = TransferStatus::ConsoleUnavailable};
		}

		std::uint8_t expectedSequence = 1;
		std::uint64_t remaining = metadata.hasSize ? metadata.sizeBytes : std::numeric_limits<std::uint64_t>::max();
		std::uint64_t written = 0;

		while (true) {
			auto status = receivePacket(context, packet, packetTimeoutMs);
			if (status == TransferStatus::CrcError) {
				(void) writeByte(context, nak);
				continue;
			}
			if (status != TransferStatus::Ok) {
				sendCancel(context);
				return {.status = status, .bytes = written};
			}

			if (packet.eot) {
				(void) writeByte(context, nak);
				status = receivePacket(context, packet, packetTimeoutMs);
				if (status != TransferStatus::Ok || !packet.eot) {
					sendCancel(context);
					return {.status = status == TransferStatus::Ok ? TransferStatus::ProtocolError : status, .bytes = written};
				}

				if (!writeByte(context, ack) || !writeByte(context, crcRequest)) {
					return {.status = TransferStatus::ConsoleUnavailable, .bytes = written};
				}

				status = receivePacket(context, packet, packetTimeoutMs);
				if (status != TransferStatus::Ok || packet.eot || packet.sequence != 0) {
					sendCancel(context);
					return {.status = status == TransferStatus::Ok ? TransferStatus::ProtocolError : status, .bytes = written};
				}
				(void) writeByte(context, ack);
				return {.status = TransferStatus::Ok, .bytes = written};
			}

			if (packet.sequence == static_cast<std::uint8_t>(expectedSequence - 1U)) {
				(void) writeByte(context, ack);
				continue;
			}
			if (packet.sequence != expectedSequence) {
				sendCancel(context);
				return {.status = TransferStatus::ProtocolError, .bytes = written};
			}

			auto bytesToWrite = packet.dataSize;
			if (metadata.hasSize) {
				bytesToWrite = static_cast<std::size_t>(
				    std::min<std::uint64_t>(remaining, static_cast<std::uint64_t>(packet.dataSize)));
			}

			if (bytesToWrite != 0) {
				auto const writeStatus = file.write(std::string_view{
				    reinterpret_cast<char const*>(packet.data.data()),
				    bytesToWrite,
				});
				if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
					sendCancel(context);
					return {.status = TransferStatus::WriteFailed, .bytes = written, .writeStatus = writeStatus};
				}
				written += bytesToWrite;
				if (metadata.hasSize) {
					remaining -= bytesToWrite;
				}
			}

			(void) writeByte(context, ack);
			++expectedSequence;
		}
	}

	bool buildMetadataPacket(
	    std::string_view transmittedName,
	    std::uint64_t fileSize,
	    std::uint8_t* output,
	    std::size_t outputSize) {
		if (transmittedName.empty() || transmittedName.size() + 2U >= outputSize) {
			return false;
		}

		std::fill(output, output + outputSize, 0);
		std::memcpy(output, transmittedName.data(), transmittedName.size());
		std::size_t position = transmittedName.size() + 1U;
		return appendDecimal(reinterpret_cast<char*>(output), outputSize, fileSize, position);
	}

	TransferResult sendYmodem(
	    lexxos::os::AppContext& context,
	    lexxos::vfs::FileHandle& file,
	    std::string_view transmittedName) {
		RawConsoleModeGuard rawMode;

		auto status = waitForReceiverRequest(context, receiverWaitMs);
		if (status != TransferStatus::Ok) {
			sendCancel(context);
			return {.status = status};
		}

		PacketData packet{};
		auto const fileSize = file.size();
		if (!buildMetadataPacket(transmittedName, fileSize, packet.data(), shortPacketBytes)) {
			sendCancel(context);
			return {.status = TransferStatus::ProtocolError};
		}

		status = sendPacketWithRetry(context, 0, packet.data(), shortPacketBytes);
		if (status != TransferStatus::Ok) {
			return {.status = status};
		}

		status = waitForReceiverRequest(context, packetTimeoutMs);
		if (status != TransferStatus::Ok) {
			sendCancel(context);
			return {.status = status};
		}

		std::uint8_t sequence = 1;
		std::uint64_t sent = 0;
		while (sent < fileSize) {
			std::fill(packet.begin(), packet.end(), cpmEof);
			std::size_t filled = 0;
			while (filled < packet.size() && sent + filled < fileSize) {
				std::size_t bytesRead = 0;
				auto const readStatus = file.read(packet.data() + filled, packet.size() - filled, bytesRead);
				if (readStatus != lexxos::vfs::ReadStatus::Ok) {
					sendCancel(context);
					return {.status = TransferStatus::ReadFailed, .bytes = sent + filled, .readStatus = readStatus};
				}
				if (bytesRead == 0) {
					break;
				}
				filled += bytesRead;
			}

			if (filled == 0) {
				break;
			}

			status = sendPacketWithRetry(context, sequence, packet.data(), packet.size());
			if (status != TransferStatus::Ok) {
				return {.status = status, .bytes = sent};
			}
			sent += filled;
			++sequence;
		}

		if (!writeByte(context, eot)) {
			return {.status = TransferStatus::ConsoleUnavailable, .bytes = sent};
		}

		std::uint8_t response{};
		status = readByte(context, response, packetTimeoutMs);
		if (status != TransferStatus::Ok) {
			sendCancel(context);
			return {.status = status, .bytes = sent};
		}
		if (response == nak) {
			if (!writeByte(context, eot)) {
				return {.status = TransferStatus::ConsoleUnavailable, .bytes = sent};
			}
			status = readByte(context, response, packetTimeoutMs);
			if (status != TransferStatus::Ok) {
				sendCancel(context);
				return {.status = status, .bytes = sent};
			}
		}
		if (response != ack) {
			sendCancel(context);
			return {.status = response == can ? TransferStatus::RemoteCancelled : TransferStatus::ProtocolError, .bytes = sent};
		}

		status = waitForReceiverRequest(context, packetTimeoutMs);
		if (status != TransferStatus::Ok) {
			sendCancel(context);
			return {.status = status, .bytes = sent};
		}

		std::fill(packet.begin(), packet.end(), 0);
		status = sendPacketWithRetry(context, 0, packet.data(), shortPacketBytes);
		if (status != TransferStatus::Ok) {
			return {.status = status, .bytes = sent};
		}

		return {.status = TransferStatus::Ok, .bytes = sent};
	}

	void writeTransferError(
	    lexxos::os::AppContext& context,
	    std::string_view commandName,
	    TransferResult const& result) {
		context.write(commandName);
		context.write(": ");
		context.write(toString(result.status));
		if (result.status == TransferStatus::OpenFailed) {
			context.write(": ");
			context.write(lexxos::vfs::toString(result.openStatus));
		} else if (result.status == TransferStatus::ReadFailed) {
			context.write(": ");
			context.write(lexxos::vfs::toString(result.readStatus));
		} else if (result.status == TransferStatus::WriteFailed) {
			context.write(": ");
			context.write(lexxos::vfs::toString(result.writeStatus));
		}
		context.writeLine("");
	}

} // namespace

ProgramDescriptor YmodemReceiveProgram::descriptor() {
	return {
	    .name = "yrx",
	    .help = "receive a file over YMODEM/CRC",
	    .description = "Receives one YMODEM/CRC file from the raw UART/USB console and writes it to VFS.",
	    .usage = "yrx [path]",
	    .arguments = argumentList(receiveArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = 0,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int YmodemReceiveProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const targetPath = arguments.positionalOr("path", {});
	context.writeLine("yrx: waiting for YMODEM sender");
	context.writeLine("yrx: host example: sb local-file.bin < /dev/ttyACM0 > /dev/ttyACM0");

	auto const result = receiveYmodem(context, targetPath);
	if (result.status != TransferStatus::Ok) {
		writeTransferError(context, "yrx", result);
		return 1;
	}

	context.write("yrx: received ");
	lexxos::apps::writeUnsigned64(context, result.bytes);
	context.writeLine(" bytes");
	return 0;
}

ProgramDescriptor YmodemSendProgram::descriptor() {
	return {
	    .name = "ysend",
	    .help = "send a VFS file over YMODEM/CRC",
	    .description = "Sends one VFS file to the raw UART/USB console using YMODEM/CRC.",
	    .usage = "ysend <path> [name]",
	    .arguments = argumentList(sendArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = 0,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int YmodemSendProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer path{};
	if (!resolvePath(context, arguments.positional("path"), path)) {
		return 1;
	}

	auto transmittedName = arguments.positionalOr("name", {});
	if (transmittedName.empty()) {
		transmittedName = basenameOf(path.data());
	}
	if (!isSafeReceivedName(transmittedName)) {
		context.writeLine("ysend: invalid transmitted file name");
		return 1;
	}

	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path.data(), lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("ysend: ");
		context.write(path.data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	context.writeLine("ysend: waiting for YMODEM receiver");
	context.writeLine("ysend: host example: rb < /dev/ttyACM0 > /dev/ttyACM0");

	auto const result = sendYmodem(context, file, transmittedName);
	if (result.status != TransferStatus::Ok) {
		writeTransferError(context, "ysend", result);
		return 1;
	}

	context.write("ysend: sent ");
	lexxos::apps::writeUnsigned64(context, result.bytes);
	context.writeLine(" bytes");
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
