#include <lexxos/apps/network/detail/hosted_ota_support.h>

namespace lexxos::apps::network_programs::detail::hosted {

using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

constexpr std::uint8_t espImageMagic = 0xE9;
constexpr std::uint64_t progressStepBytes = 64U * 1024U;
constexpr std::uint8_t ymodemSoh = 0x01;
constexpr std::uint8_t ymodemStx = 0x02;
constexpr std::uint8_t ymodemEot = 0x04;
constexpr std::uint8_t ymodemAck = 0x06;
constexpr std::uint8_t ymodemNak = 0x15;
constexpr std::uint8_t ymodemCan = 0x18;
constexpr std::uint8_t ymodemCrcRequest = static_cast<std::uint8_t>('C');
constexpr std::size_t ymodemShortPacketBytes = 128;
constexpr std::size_t ymodemLongPacketBytes = 1024;
constexpr std::uint32_t ymodemStartupTimeoutMs = 2000;
constexpr std::uint32_t ymodemPacketTimeoutMs = 10000;
constexpr std::uint32_t ymodemByteTimeoutMs = 1000;

template <typename T>
bool allocateWorkspaceObject(lexxos::os::AppContext& context, char const* programName, T*& output) {
	auto* const arena = context.memoryArena();
	if (arena == nullptr) {
		context.write(programName);
		context.writeLine(": program memory arena is unavailable");
		return false;
	}

	auto* const memory = arena->allocate(sizeof(T), alignof(T));
	if (memory == nullptr) {
		context.write(programName);
		context.writeLine(": not enough program workspace memory");
		return false;
	}

	output = new (memory) T{};
	return true;
}

bool allocateWorkspace(lexxos::os::AppContext& context, PathBuffer*& path, OtaBuffer*& buffer) {
	return allocateWorkspaceObject(context, "hosted-ota", path) && allocateWorkspaceObject(context, "hosted-ota", buffer);
}

#if defined(LEXXOS_PLATFORM_ESPIDF)

struct YmodemPacket {
	bool eot{false};
	std::uint8_t sequence{0};
	std::size_t dataSize{0};
	std::array<std::uint8_t, ymodemLongPacketBytes> data{};
};

struct YmodemMetadata {
	bool empty{true};
	bool hasSize{false};
	std::uint64_t sizeBytes{0};
};

class RawConsoleModeGuard final {
public:
	RawConsoleModeGuard(lexxos::app::ConsoleInputService& input)
	    : _input(input)
	    , _previous(input.mode()) {
		_input.setMode(lexxos::app::console_input::Mode::RawProgram);
	}

	RawConsoleModeGuard(RawConsoleModeGuard const&) = delete;
	RawConsoleModeGuard& operator=(RawConsoleModeGuard const&) = delete;

	~RawConsoleModeGuard() {
		_input.setMode(_previous);
	}

private:
	lexxos::app::ConsoleInputService& _input;
	lexxos::app::console_input::Mode _previous;
};

void writeEspError(lexxos::os::AppContext& context, char const* prefix, esp_err_t error) {
	context.write(prefix);
	context.writeLine(esp_err_to_name(error));
}

void writeVersion(lexxos::os::AppContext& context, lexxos::devices::espidf_hosted::Version version) {
	if (!version.valid) {
		context.write("-");
		return;
	}

	writeUnsigned(context, version.major);
	context.write(".");
	writeUnsigned(context, version.minor);
	context.write(".");
	writeUnsigned(context, version.patch);
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

void sendYmodemCancel(lexxos::os::AppContext& context) {
	(void) writeByte(context, ymodemCan);
	(void) writeByte(context, ymodemCan);
}

OtaRxStatus readByte(
    lexxos::os::AppContext& context,
    std::uint8_t& value,
    std::uint32_t timeoutMs) {
	auto const started = uptimeMs(context);
	while (true) {
		char raw{};
		if (context.consoleInput().readChar(context.platform(), raw)) {
			value = static_cast<std::uint8_t>(raw);
			return OtaRxStatus::Ok;
		}

		if (context.cancellationRequested()) {
			return OtaRxStatus::Cancelled;
		}

		if (elapsed(started, uptimeMs(context), timeoutMs)) {
			return OtaRxStatus::Timeout;
		}

		sleepMs(context, 2);
	}
}

OtaRxStatus readRequired(
    lexxos::os::AppContext& context,
    std::uint8_t* output,
    std::size_t size,
    std::uint32_t timeoutMs = ymodemByteTimeoutMs) {
	for (std::size_t index = 0; index < size; ++index) {
		auto const status = readByte(context, output[index], timeoutMs);
		if (status != OtaRxStatus::Ok) {
			return status;
		}
	}

	return OtaRxStatus::Ok;
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

OtaRxStatus receiveYmodemPacket(
    lexxos::os::AppContext& context,
    YmodemPacket& packet,
    std::uint32_t timeoutMs) {
	packet = {};

	std::uint8_t start{};
	auto status = readByte(context, start, timeoutMs);
	if (status != OtaRxStatus::Ok) {
		return status;
	}

	if (start == ymodemEot) {
		packet.eot = true;
		return OtaRxStatus::Ok;
	}

	if (start == ymodemCan) {
		return OtaRxStatus::RemoteCancelled;
	}

	if (start != ymodemSoh && start != ymodemStx) {
		return OtaRxStatus::ProtocolError;
	}

	packet.dataSize = start == ymodemSoh ? ymodemShortPacketBytes : ymodemLongPacketBytes;
	std::uint8_t header[2]{};
	status = readRequired(context, header, sizeof(header));
	if (status != OtaRxStatus::Ok) {
		return status;
	}

	packet.sequence = header[0];
	if (static_cast<std::uint8_t>(header[0] + header[1]) != 0xFFU) {
		return OtaRxStatus::ProtocolError;
	}

	status = readRequired(context, packet.data.data(), packet.dataSize);
	if (status != OtaRxStatus::Ok) {
		return status;
	}

	std::uint8_t crcBytes[2]{};
	status = readRequired(context, crcBytes, sizeof(crcBytes));
	if (status != OtaRxStatus::Ok) {
		return status;
	}

	auto const receivedCrc =
	    static_cast<std::uint16_t>((static_cast<std::uint16_t>(crcBytes[0]) << 8U) | crcBytes[1]);
	if (receivedCrc != crc16Xmodem(packet.data.data(), packet.dataSize)) {
		return OtaRxStatus::CrcError;
	}

	return OtaRxStatus::Ok;
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

YmodemMetadata parseYmodemMetadata(YmodemPacket const& packet) {
	YmodemMetadata metadata{};
	if (packet.dataSize == 0U || packet.data[0] == 0) {
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

char const* toString(OtaRxStatus status) {
	switch (status) {
		case OtaRxStatus::Ok:
			return "ok";
		case OtaRxStatus::Timeout:
			return "timeout";
		case OtaRxStatus::Cancelled:
			return "cancelled";
		case OtaRxStatus::RemoteCancelled:
			return "remote cancelled";
		case OtaRxStatus::ProtocolError:
			return "protocol error";
		case OtaRxStatus::CrcError:
			return "crc error";
		case OtaRxStatus::ConsoleUnavailable:
			return "console is unavailable";
		case OtaRxStatus::EmptyBatch:
			return "empty YMODEM batch";
		case OtaRxStatus::InvalidImageMagic:
			return "invalid ESP image magic";
		case OtaRxStatus::OtaWriteFailed:
			return "OTA write failed";
	}

	return "unknown";
}

OtaRxResult receiveYmodemHostedOta(lexxos::os::AppContext& context, bool dryRun) {
	RawConsoleModeGuard rawMode{context.consoleInput()};

	YmodemPacket packet{};
	YmodemMetadata metadata{};
	bool metadataReady = false;
	for (std::uint8_t attempt = 0; attempt < 30U && !metadataReady; ++attempt) {
		if (!writeByte(context, ymodemCrcRequest)) {
			return {.status = OtaRxStatus::ConsoleUnavailable};
		}

		auto const status = receiveYmodemPacket(context, packet, ymodemStartupTimeoutMs);
		if (status == OtaRxStatus::Timeout) {
			continue;
		}

		if (status != OtaRxStatus::Ok || packet.eot || packet.sequence != 0U) {
			sendYmodemCancel(context);
			return {.status = status == OtaRxStatus::Ok ? OtaRxStatus::ProtocolError : status};
		}

		metadata = parseYmodemMetadata(packet);
		if (metadata.empty) {
			(void) writeByte(context, ymodemAck);
			return {.status = OtaRxStatus::EmptyBatch};
		}

		metadataReady = true;
	}

	if (!metadataReady) {
		sendYmodemCancel(context);
		return {.status = OtaRxStatus::Timeout};
	}

	if (metadata.hasSize && metadata.sizeBytes == 0U) {
		sendYmodemCancel(context);
		return {.status = OtaRxStatus::EmptyBatch, .hasExpectedBytes = true};
	}

	if (!writeByte(context, ymodemAck) || !writeByte(context, ymodemCrcRequest)) {
		return {.status = OtaRxStatus::ConsoleUnavailable};
	}

	OtaRxResult result{};
	result.hasExpectedBytes = metadata.hasSize;
	result.expectedBytes = metadata.hasSize ? metadata.sizeBytes : 0U;
	std::uint8_t expectedSequence = 1;
	std::uint64_t remaining = metadata.hasSize ? metadata.sizeBytes : std::numeric_limits<std::uint64_t>::max();

	while (true) {
		auto status = receiveYmodemPacket(context, packet, ymodemPacketTimeoutMs);
		if (status == OtaRxStatus::CrcError) {
			(void) writeByte(context, ymodemNak);
			continue;
		}

		if (status != OtaRxStatus::Ok) {
			sendYmodemCancel(context);
			result.status = status;
			return result;
		}

		if (packet.eot) {
			(void) writeByte(context, ymodemNak);
			status = receiveYmodemPacket(context, packet, ymodemPacketTimeoutMs);
			if (status != OtaRxStatus::Ok || !packet.eot) {
				sendYmodemCancel(context);
				result.status = status == OtaRxStatus::Ok ? OtaRxStatus::ProtocolError : status;
				return result;
			}

			if (!writeByte(context, ymodemAck) || !writeByte(context, ymodemCrcRequest)) {
				result.status = OtaRxStatus::ConsoleUnavailable;
				return result;
			}

			status = receiveYmodemPacket(context, packet, ymodemPacketTimeoutMs);
			if (status != OtaRxStatus::Ok || packet.eot || packet.sequence != 0U) {
				sendYmodemCancel(context);
				result.status = status == OtaRxStatus::Ok ? OtaRxStatus::ProtocolError : status;
				return result;
			}

			(void) writeByte(context, ymodemAck);
			result.status = OtaRxStatus::Ok;
			return result;
		}

		if (packet.sequence == static_cast<std::uint8_t>(expectedSequence - 1U)) {
			(void) writeByte(context, ymodemAck);
			continue;
		}

		if (packet.sequence != expectedSequence) {
			sendYmodemCancel(context);
			result.status = OtaRxStatus::ProtocolError;
			return result;
		}

		auto bytesToWrite = packet.dataSize;
		if (metadata.hasSize) {
			bytesToWrite = static_cast<std::size_t>(
			    std::min<std::uint64_t>(remaining, static_cast<std::uint64_t>(packet.dataSize)));
		}

		if (bytesToWrite != 0U) {
			if (result.bytes == 0U) {
				result.firstByte = packet.data[0];
				if (result.firstByte != espImageMagic) {
					sendYmodemCancel(context);
					result.status = OtaRxStatus::InvalidImageMagic;
					return result;
				}
			}

			if (!dryRun) {
				auto const otaError =
				    esp_hosted_slave_ota_write(packet.data.data(), static_cast<std::uint32_t>(bytesToWrite));
				if (otaError != ESP_OK) {
					sendYmodemCancel(context);
					result.status = OtaRxStatus::OtaWriteFailed;
					result.otaError = otaError;
					return result;
				}
			}

			result.bytes += bytesToWrite;
			if (metadata.hasSize) {
				remaining -= bytesToWrite;
			}
		}

		(void) writeByte(context, ymodemAck);
		++expectedSequence;
	}
}

bool readFirstChunk(
    lexxos::os::AppContext& context,
    lexxos::vfs::FileHandle& file,
    char const* path,
    OtaBuffer& buffer,
    std::size_t& bytesRead) {
	auto const readStatus = file.read(buffer.data(), buffer.size(), bytesRead);
	if (readStatus != lexxos::vfs::ReadStatus::Ok) {
		context.write("hosted-ota: ");
		context.write(path);
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(readStatus));
		return false;
	}

	if (bytesRead == 0U) {
		context.writeLine("hosted-ota: source file is empty");
		return false;
	}

	if (buffer[0] != espImageMagic) {
		context.write("hosted-ota: invalid ESP image magic 0x");
		writeHexByte(context, buffer[0]);
		context.writeLine(", expected 0xE9");
		return false;
	}

	return true;
}

bool writeChunk(lexxos::os::AppContext& context, OtaBuffer& buffer, std::size_t bytesRead) {
	auto const result = esp_hosted_slave_ota_write(buffer.data(), static_cast<std::uint32_t>(bytesRead));
	if (result != ESP_OK) {
		writeEspError(context, "hosted-ota: write failed: ", result);
		return false;
	}

	return true;
}

bool streamFirmware(
    lexxos::os::AppContext& context,
    lexxos::vfs::FileHandle& file,
    char const* path,
    std::uint64_t fileSize,
    OtaBuffer& buffer,
    std::size_t firstBytesRead,
    std::uint64_t& bytesWritten) {
	bytesWritten = 0;
	std::uint64_t nextProgress = progressStepBytes;

	if (!writeChunk(context, buffer, firstBytesRead)) {
		return false;
	}

	bytesWritten += firstBytesRead;

	while (bytesWritten < fileSize) {
		std::size_t bytesRead = 0;
		auto const readStatus = file.read(buffer.data(), buffer.size(), bytesRead);
		if (readStatus != lexxos::vfs::ReadStatus::Ok) {
			context.write("hosted-ota: ");
			context.write(path);
			context.write(": ");
			context.writeLine(lexxos::vfs::toString(readStatus));
			return false;
		}

		if (bytesRead == 0U) {
			context.writeLine("hosted-ota: unexpected end of file");
			return false;
		}

		if (!writeChunk(context, buffer, bytesRead)) {
			return false;
		}

		bytesWritten += bytesRead;
		if (bytesWritten >= nextProgress || bytesWritten >= fileSize) {
			context.write("hosted-ota: wrote ");
			writeUnsigned64(context, bytesWritten);
			context.write("/");
			writeUnsigned64(context, fileSize);
			context.writeLine(" bytes");
			while (nextProgress <= bytesWritten) {
				nextProgress += progressStepBytes;
			}
		}
	}

	return true;
}
#endif

} // namespace lexxos::apps::network_programs::detail::hosted
