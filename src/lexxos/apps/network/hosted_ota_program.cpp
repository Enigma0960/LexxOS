#include <lexxos/apps/network/hosted_ota_program.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <new>
#include <string_view>

#include <lexxos/app/console_input.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <esp_err.h>
	#include <esp_hosted.h>
	#include <esp_system.h>
	#include <lexxos/devices/espidf/espidf_hosted.h>
#endif

namespace lexxos::apps::network_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramArgumentValueType;
	using lexxos::program::ProgramDescriptor;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using OtaBuffer = std::array<std::uint8_t, 1400>;

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
	constexpr std::uint32_t hostedOtaWorkspaceBytes =
	    static_cast<std::uint32_t>(sizeof(PathBuffer) + alignof(PathBuffer) + sizeof(OtaBuffer) + alignof(OtaBuffer));

	constexpr ProgramArgumentSpec hostedOtaArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "VFS path to ESP32-C6 network_adapter.bin",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::Path,
         .required = true,
	     },
	    {
         .name = "dry-run",
         .help = "validate the source file and hosted link without writing C6 flash",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "no-activate",
         .help = "skip OTA activate even when the current C6 firmware supports it",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "reboot-host",
         .help = "reboot ESP32-P4 after a successful OTA",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	};

	constexpr ProgramArgumentSpec hostedOtaRxArguments[] = {
	    {
         .name = "dry-run",
         .help = "receive and validate the YMODEM image without writing C6 flash",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "no-activate",
         .help = "skip OTA activate even when the current C6 firmware supports it",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "reboot-host",
         .help = "reboot ESP32-P4 after a successful OTA",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	};

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
	enum class OtaRxStatus : std::uint8_t {
		Ok,
		Timeout,
		Cancelled,
		RemoteCancelled,
		ProtocolError,
		CrcError,
		ConsoleUnavailable,
		EmptyBatch,
		InvalidImageMagic,
		OtaWriteFailed,
	};

	struct OtaRxResult {
		OtaRxStatus status{OtaRxStatus::Ok};
		std::uint64_t bytes{0};
		std::uint64_t expectedBytes{0};
		bool hasExpectedBytes{false};
		std::uint8_t firstByte{0};
		esp_err_t otaError{ESP_OK};
	};

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
			if (lexxos::app::console_input::readChar(context.platform(), raw)) {
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
		RawConsoleModeGuard rawMode;

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

} // namespace

ProgramDescriptor HostedOtaProgram::descriptor() {
	return {
	    .name = "hosted-ota",
	    .help = "update ESP-Hosted C6 firmware from a VFS file",
	    .description = "Reads an ESP32-C6 network_adapter.bin from VFS and sends it to the ESP-Hosted coprocessor over the active board transport.",
	    .usage = "hosted-ota [--dry-run] [--no-activate] [--reboot-host] <path>",
	    .arguments = argumentList(hostedOtaArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = hostedOtaWorkspaceBytes,
	                .cancellable = false,
	                .allowBackground = false,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int HostedOtaProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	PathBuffer* path = nullptr;
	OtaBuffer* buffer = nullptr;
	if (!allocateWorkspace(context, path, buffer)) {
		return 1;
	}
	if (!resolvePath(context, arguments.positional("path"), *path)) {
		return 1;
	}

	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path->data(), lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("hosted-ota: ");
		context.write(path->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	auto const fileSize = file.size();
	if (fileSize == 0U) {
		context.writeLine("hosted-ota: source file is empty");
		return 1;
	}

	std::size_t firstBytesRead = 0;
	if (!readFirstChunk(context, file, path->data(), *buffer, firstBytesRead)) {
		return 1;
	}

	auto const& board = activeBoard(context);
	auto const info = lexxos::devices::espidf_hosted::readInfo(board, true);
	if (!info.supported) {
		context.writeLine("hosted-ota: active board does not use ESP-Hosted");
		return 1;
	}
	if (!info.backendStarted) {
		context.write("hosted-ota: hosted backend did not start: ");
		context.writeLine(info.backendStartErrorName);
		return 1;
	}

	context.write("hosted-ota: source=");
	context.write(path->data());
	context.write(" size=");
	writeUnsigned64(context, fileSize);
	context.writeLine(" bytes");
	context.write("hosted-ota: host-version=");
	writeVersion(context, info.hostVersion);
	context.write(" coprocessor-version=");
	writeVersion(context, info.coprocessorVersion);
	context.write(" compatible=");
	context.writeLine(info.compatibilityKnown ? (info.majorMinorCompatible ? "yes" : "no") : "unknown");

	if (arguments.has("dry-run")) {
		context.writeLine("hosted-ota: dry run passed");
		return 0;
	}

	context.writeLine("hosted-ota: begin");
	auto result = esp_hosted_slave_ota_begin();
	if (result != ESP_OK) {
		writeEspError(context, "hosted-ota: begin failed: ", result);
		return 1;
	}

	std::uint64_t bytesWritten = 0;
	if (!streamFirmware(context, file, path->data(), fileSize, *buffer, firstBytesRead, bytesWritten)) {
		return 1;
	}

	result = esp_hosted_slave_ota_end();
	if (result != ESP_OK) {
		writeEspError(context, "hosted-ota: end failed: ", result);
		return 1;
	}
	context.writeLine("hosted-ota: end ok");

	auto const canActivate = lexxos::devices::espidf_hosted::activateSupported(info.coprocessorVersion);
	if (canActivate && !arguments.has("no-activate")) {
		context.writeLine("hosted-ota: activate");
		result = esp_hosted_slave_ota_activate();
		if (result != ESP_OK) {
			writeEspError(context, "hosted-ota: activate failed: ", result);
			return 1;
		}
		context.writeLine("hosted-ota: activate ok");
	} else if (arguments.has("no-activate")) {
		context.writeLine("hosted-ota: activate skipped by --no-activate");
	} else {
		context.writeLine("hosted-ota: activate skipped; current C6 firmware is older than 2.6.0");
	}

	context.writeLine("hosted-ota: update completed");
	if (arguments.has("reboot-host")) {
		context.writeLine("hosted-ota: rebooting host");
		esp_restart();
	} else {
		context.writeLine("hosted-ota: reboot host before using Wi-Fi again");
	}
	return 0;
#else
	(void) arguments;
	context.writeErrorLine("hosted-ota: ESP-Hosted support is unavailable in this build");
	return 1;
#endif
}

ProgramDescriptor HostedOtaRxProgram::descriptor() {
	return {
	    .name = "hosted-ota-rx",
	    .help = "update ESP-Hosted C6 firmware from UART/YMODEM",
	    .description = "Receives an ESP32-C6 network_adapter.bin over the raw console with YMODEM and streams it directly to the ESP-Hosted coprocessor OTA RPC.",
	    .usage = "hosted-ota-rx [--dry-run] [--no-activate] [--reboot-host]",
	    .arguments = argumentList(hostedOtaRxArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = static_cast<std::uint32_t>(sizeof(OtaBuffer) + alignof(OtaBuffer)),
	                .cancellable = false,
	                .allowBackground = false,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int HostedOtaRxProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	auto const dryRun = arguments.has("dry-run");

	auto const& board = activeBoard(context);
	auto const info = lexxos::devices::espidf_hosted::readInfo(board, true);
	if (!info.supported) {
		context.writeLine("hosted-ota-rx: active board does not use ESP-Hosted");
		return 1;
	}
	if (!info.backendStarted) {
		context.write("hosted-ota-rx: hosted backend did not start: ");
		context.writeLine(info.backendStartErrorName);
		return 1;
	}

	context.write("hosted-ota-rx: host-version=");
	writeVersion(context, info.hostVersion);
	context.write(" coprocessor-version=");
	writeVersion(context, info.coprocessorVersion);
	context.write(" compatible=");
	context.writeLine(info.compatibilityKnown ? (info.majorMinorCompatible ? "yes" : "no") : "unknown");

	if (!dryRun) {
		context.writeLine("hosted-ota-rx: begin");
		auto const beginResult = esp_hosted_slave_ota_begin();
		if (beginResult != ESP_OK) {
			writeEspError(context, "hosted-ota-rx: begin failed: ", beginResult);
			return 1;
		}
	}

	context.writeLine("hosted-ota-rx: waiting for YMODEM sender");
	context.writeLine("hosted-ota-rx: host example: ymodem.send_file(serial, network_adapter.bin)");
	auto const receiveResult = receiveYmodemHostedOta(context, dryRun);
	if (receiveResult.status != OtaRxStatus::Ok) {
		context.write("hosted-ota-rx: ");
		context.write(toString(receiveResult.status));
		if (receiveResult.status == OtaRxStatus::InvalidImageMagic) {
			context.write(": 0x");
			writeHexByte(context, receiveResult.firstByte);
			context.write(", expected 0xE9");
		} else if (receiveResult.status == OtaRxStatus::OtaWriteFailed) {
			context.write(": ");
			context.write(esp_err_to_name(receiveResult.otaError));
		}
		context.writeLine("");
		return 1;
	}

	context.write("hosted-ota-rx: received ");
	writeUnsigned64(context, receiveResult.bytes);
	if (receiveResult.hasExpectedBytes) {
		context.write("/");
		writeUnsigned64(context, receiveResult.expectedBytes);
	}
	context.writeLine(" bytes");
	if (receiveResult.hasExpectedBytes && receiveResult.bytes != receiveResult.expectedBytes) {
		context.writeLine("hosted-ota-rx: received byte count does not match YMODEM metadata");
		return 1;
	}

	if (dryRun) {
		context.writeLine("hosted-ota-rx: dry run passed");
		return 0;
	}

	auto result = esp_hosted_slave_ota_end();
	if (result != ESP_OK) {
		writeEspError(context, "hosted-ota-rx: end failed: ", result);
		return 1;
	}
	context.writeLine("hosted-ota-rx: end ok");

	auto const canActivate = lexxos::devices::espidf_hosted::activateSupported(info.coprocessorVersion);
	if (canActivate && !arguments.has("no-activate")) {
		context.writeLine("hosted-ota-rx: activate");
		result = esp_hosted_slave_ota_activate();
		if (result != ESP_OK) {
			writeEspError(context, "hosted-ota-rx: activate failed: ", result);
			return 1;
		}
		context.writeLine("hosted-ota-rx: activate ok");
	} else if (arguments.has("no-activate")) {
		context.writeLine("hosted-ota-rx: activate skipped by --no-activate");
	} else {
		context.writeLine("hosted-ota-rx: activate skipped; current C6 firmware is older than 2.6.0");
	}

	context.writeLine("hosted-ota-rx: update completed");
	if (arguments.has("reboot-host")) {
		context.writeLine("hosted-ota-rx: rebooting host");
		esp_restart();
	} else {
		context.writeLine("hosted-ota-rx: reboot host before using Wi-Fi again");
	}
	return 0;
#else
	(void) arguments;
	context.writeErrorLine("hosted-ota-rx: ESP-Hosted support is unavailable in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::network_programs
