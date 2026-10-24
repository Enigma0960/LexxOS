#include <lexxos/apps/common/core_programs_support.h>

#include <limits>

#include <lexxos/feature.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

#if FEATURE_ENABLED(SD_CARD)
	#include <lexxos/devices/sd_card.h>
#endif

namespace lexxos::apps {

void writeUnsigned(lexxos::os::AppContext& shell, std::uint32_t value) {
	char buffer[16]{};
	int position = static_cast<int>(sizeof(buffer)) - 1;
	buffer[position] = '\0';

	if (value == 0) {
		shell.write("0");
		return;
	}

	while (value != 0 && position > 0) {
		--position;
		buffer[position] = static_cast<char>('0' + (value % 10));
		value /= 10;
	}

	shell.write(&buffer[position]);
}

void writeUnsigned64(lexxos::os::AppContext& shell, std::uint64_t value) {
	char buffer[32]{};
	int position = static_cast<int>(sizeof(buffer)) - 1;
	buffer[position] = '\0';

	if (value == 0) {
		shell.write("0");
		return;
	}

	while (value != 0 && position > 0) {
		--position;
		buffer[position] = static_cast<char>('0' + (value % 10));
		value /= 10;
	}
	shell.write(&buffer[position]);
}

char hexDigit(std::uint8_t value) {
	return static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
}

void writeHex8(lexxos::os::AppContext& shell, std::uint8_t value) {
	char buffer[5]{
	    '0',
	    'x',
	    hexDigit(static_cast<std::uint8_t>((value >> 4) & 0x0FU)),
	    hexDigit(static_cast<std::uint8_t>(value & 0x0FU)),
	    '\0',
	};
	shell.write(buffer);
}

void writeHexByte(lexxos::os::AppContext& shell, std::uint8_t value) {
	char buffer[3]{
	    hexDigit(static_cast<std::uint8_t>((value >> 4) & 0x0FU)),
	    hexDigit(static_cast<std::uint8_t>(value & 0x0FU)),
	    '\0',
	};
	shell.write(buffer);
}

void writeHex32(lexxos::os::AppContext& shell, std::uint32_t value) {
	for (int shift = 28; shift >= 0; shift -= 4) {
		std::uint8_t const digit = static_cast<std::uint8_t>((value >> shift) & 0x0FU);
		char const character = hexDigit(digit);
		shell.write(std::string_view{&character, 1});
	}
}

bool parseUnsigned64(std::string_view text, std::uint64_t& value) {
	if (text.empty()) {
		return false;
	}

	std::uint64_t result = 0;
	for (char const character : text) {
		if (character < '0' || character > '9') {
			return false;
		}

		std::uint64_t const digit = static_cast<std::uint64_t>(character - '0');
		if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10ULL) {
			return false;
		}
		result = result * 10ULL + digit;
	}

	value = result;
	return true;
}

bool parseUnsigned32(std::string_view text, std::uint32_t& value) {
	std::uint64_t parsed = 0;
	if (!parseUnsigned64(text, parsed) || parsed > std::numeric_limits<std::uint32_t>::max()) {
		return false;
	}
	value = static_cast<std::uint32_t>(parsed);
	return true;
}

std::uint32_t uptimeMs(lexxos::os::AppContext const& context) {
	auto const& clock = context.platform().clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

void sleepMs(lexxos::os::AppContext const& context, std::uint32_t milliseconds) {
	auto const& clock = context.platform().clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

lexxos::platform::BoardDescriptor const& activeBoard(lexxos::os::AppContext const& context) {
	auto const& board = context.platform().board;
	return board.current != nullptr ? board.current() : lexxos::platform::board();
}

bool isPrintableAscii(std::uint8_t value) {
	return value >= 0x20U && value <= 0x7EU;
}

std::uint16_t readLe16(std::uint8_t const* data) {
	return static_cast<std::uint16_t>(data[0]) | static_cast<std::uint16_t>(data[1] << 8);
}

std::uint32_t readLe32(std::uint8_t const* data) {
	return static_cast<std::uint32_t>(data[0])
	    | (static_cast<std::uint32_t>(data[1]) << 8)
	    | (static_cast<std::uint32_t>(data[2]) << 16)
	    | (static_cast<std::uint32_t>(data[3]) << 24);
}

lexxos::devices::BlockDevice* sdBlockDevice(lexxos::os::AppContext& context) {
	return context.devices().blockDevice("sd0");
}

lexxos::devices::DeviceResourceLock lockSdBlockDevice(lexxos::os::AppContext& context, std::string_view programName) {
	auto& devices = context.devices();
	if (devices.blockDevice("sd0") == nullptr) {
		context.writeError(programName);
		context.writeErrorLine(": sd0 is not registered in DeviceManager");
		return {};
	}
	auto lock = devices.lockBlockDevice("sd0", lexxos::devices::DeviceResourcePriority::Interactive, programName);
	if (!lock) {
		context.writeError(programName);
		context.writeErrorLine(": block device is busy: sd0");
	}
	return lock;
}

void writeGpioPinValue(lexxos::os::AppContext& shell, lexxos::platform::GpioPin pin) {
	if (!pin.isValid()) {
		shell.write("-");
		return;
	}
	writeUnsigned(shell, static_cast<std::uint32_t>(pin.number));
}

void writeSdioPinsSummary(lexxos::os::AppContext& shell, lexxos::platform::SdioPins const& pins) {
	if (!pins.isValid()) {
		return;
	}
	shell.write("sdio pins: clk=");
	writeGpioPinValue(shell, pins.clock);
	shell.write(" cmd=");
	writeGpioPinValue(shell, pins.command);
	shell.write(" d0=");
	writeGpioPinValue(shell, pins.data0);
	shell.write(" d1=");
	writeGpioPinValue(shell, pins.data1);
	shell.write(" d2=");
	writeGpioPinValue(shell, pins.data2);
	shell.write(" d3=");
	writeGpioPinValue(shell, pins.data3);
	shell.writeLine("");
	shell.write("sdio bus width: ");
	shell.writeLine(pins.supportsWideBus() ? "4-bit" : "1-bit");
	shell.write("sdio clocks: init=");
	writeUnsigned(shell, pins.initializationClockHz);
	shell.write(" Hz transfer=");
	writeUnsigned(shell, pins.transferClockHz);
	shell.writeLine(" Hz");
}

void writeBlockDeviceSummary(lexxos::os::AppContext& shell, lexxos::devices::BlockDevice const* device) {
	if (device == nullptr) {
		shell.writeLine("block device: missing");
		return;
	}

	shell.write("block ready: ");
	shell.writeLine(device->isReady() ? "yes" : "no");
	shell.write("block size: ");
	writeUnsigned(shell, device->blockSizeBytes());
	shell.writeLine(" bytes");
	shell.write("block count: ");
	writeUnsigned64(shell, device->blockCount());
	shell.writeLine("");

	std::uint64_t const capacityMiB = (device->blockCount() * static_cast<std::uint64_t>(device->blockSizeBytes())) / (1024ULL * 1024ULL);
	shell.write("capacity: ");
	writeUnsigned64(shell, capacityMiB);
	shell.writeLine(" MiB");
}

void writeStorageSummary(lexxos::os::AppContext& context) {
	auto& shell = context;
	auto const* device = sdBlockDevice(context);

	shell.write("block device: ");
	shell.writeLine(device != nullptr ? "sd0" : "missing");
#if FEATURE_ENABLED(SD_CARD)
	shell.write("card detect: ");
	if (lexxos::devices::sd_card::hasCardDetect()) {
		shell.writeLine(lexxos::devices::sd_card::isCardInserted() ? "inserted" : "empty");
	} else {
		shell.writeLine("not-configured");
	}

	auto const boardSdInterface = lexxos::devices::sd_card::boardInterface(context.platform());
	if (boardSdInterface != lexxos::platform::SdInterface::None) {
		shell.write("sd board interface: ");
		shell.writeLine(lexxos::platform::toString(boardSdInterface));
		if (lexxos::platform::isSdioInterface(boardSdInterface)) {
			writeSdioPinsSummary(shell, activeBoard(context).sdSdio);
	#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_SD_BACKEND_SDIO)
			shell.writeLine("sdio implementation: rp2040 experimental skeleton");
	#endif
		}
	}
	shell.write("sd interface: ");
	shell.writeLine(lexxos::devices::toString(lexxos::devices::sd_card::interfaceType()));
	if (!lexxos::devices::sd_card::backendMatchesBoardInterface(context.platform())) {
		shell.writeLine("sd backend warning: selected backend does not match board descriptor");
	}
	shell.write("sd state: ");
	shell.writeLine(lexxos::devices::toString(lexxos::devices::sd_card::state()));
	shell.write("sd type: ");
	shell.writeLine(lexxos::devices::toString(lexxos::devices::sd_card::cardType()));
	shell.write("active baud: ");
	writeUnsigned(shell, lexxos::devices::sd_card::activeBaudRate());
	shell.writeLine(" Hz");
	shell.write("spi mode: ");
	writeUnsigned(shell, static_cast<std::uint32_t>(lexxos::devices::sd_card::spiClockMode()));
	shell.writeLine("");
	shell.write("read crc: ");
	shell.writeLine(lexxos::devices::sd_card::readCrcValidation() ? "on" : "off");
	shell.write("multi-block read: ");
	shell.writeLine(lexxos::devices::sd_card::multiBlockReadEnabled() ? "on" : "off");
	shell.write("multi-block write: ");
	shell.writeLine(lexxos::devices::sd_card::multiBlockWriteEnabled() ? "on" : "off");
	shell.write("spi dma: ");
	shell.writeLine(lexxos::devices::sd_card::dmaSupported() ? "yes" : "no");
	shell.write("sd dma transfers: ");
	shell.writeLine(lexxos::devices::sd_card::dmaTransfersEnabled() ? "on" : "off");
	shell.write("high-speed writes: ");
	shell.writeLine(lexxos::devices::sd_card::highSpeedWritesEnabled() ? "enabled" : "guarded");
	shell.write("read recoveries: ");
	writeUnsigned(shell, lexxos::devices::sd_card::readRecoveryReinitializations());
	shell.writeLine("");
#else
	shell.writeLine("sd support: disabled");
#endif

	writeBlockDeviceSummary(shell, device);
}

void writeTmpSummary(lexxos::os::AppContext& context, lexxos::vfs::TmpInfo const& info) {
	context.write("files: ");
	writeUnsigned64(context, info.usedFiles);
	context.write("/");
	writeUnsigned64(context, info.maxFiles);
	context.write(" used, ");
	writeUnsigned64(context, info.freeFiles);
	context.writeLine(" free");

	context.write("bytes: ");
	writeUnsigned64(context, info.usedBytes);
	context.write("/");
	writeUnsigned64(context, info.totalBytes);
	context.write(" used, ");
	writeUnsigned64(context, info.freeBytes);
	context.writeLine(" free");

	context.write("max file size: ");
	writeUnsigned64(context, info.maxFileSizeBytes);
	context.writeLine(" bytes");
}

bool probeStorage(lexxos::os::AppContext& context, std::uint8_t attempts) {
	auto& shell = context;
#if FEATURE_ENABLED(SD_CARD)
	shell.writeLine("sd probe: running");
	if (lexxos::devices::sd_card::probe(context.platform(), attempts)) {
		shell.writeLine("sd probe: ready");
		return true;
	}

	shell.write("sd probe: failed, state=");
	shell.writeLine(lexxos::devices::toString(lexxos::devices::sd_card::state()));
#else
	(void) attempts;
	shell.writeLine("sd probe: SD feature is disabled");
#endif
	return false;
}

bool ensureStorageReady(lexxos::os::AppContext& context) {
	auto* device = sdBlockDevice(context);
	return (device != nullptr && device->isReady()) || probeStorage(context, storageProbeAttempts);
}

bool readRawStorageBlock0(lexxos::os::AppContext& context, StorageBlock& sector) {
	auto* device = sdBlockDevice(context);
	return device != nullptr && device->isReady() && device->readBlocks(0, sector.data(), 1);
}

bool isSdBlockSource(std::string_view source) {
	return source == "sd0" || source == "/dev/sd0";
}

bool prepareSdCardForMount(lexxos::os::AppContext& context) {
	auto& shell = context;
#if FEATURE_DISABLED(SD_CARD)
	shell.writeLine("sd mount: SD feature is disabled");
	return false;
#else
	shell.writeLine("sd mount: probing");
	if (!lexxos::devices::sd_card::probe(context.platform(), mountProbeAttempts)) {
		shell.write("sd mount: probe failed, state=");
		shell.writeLine(lexxos::devices::toString(lexxos::devices::sd_card::state()));
		return false;
	}

	StorageBlock sector{};
	if (readRawStorageBlock0(context, sector)) {
		shell.writeLine("sd mount: block0 ok");
		return true;
	}

	shell.writeLine("sd mount: block0 read failed, retrying probe");
	if (!lexxos::devices::sd_card::probe(context.platform(), mountProbeAttempts)) {
		shell.write("sd mount: probe failed, state=");
		shell.writeLine(lexxos::devices::toString(lexxos::devices::sd_card::state()));
		return false;
	}

	if (readRawStorageBlock0(context, sector)) {
		shell.writeLine("sd mount: block0 ok");
		return true;
	}

	shell.write("sd mount: block0 read failed, state=");
	shell.writeLine(lexxos::devices::toString(lexxos::devices::sd_card::state()));
	return false;
#endif
}

bool readStorageBlock0(lexxos::os::AppContext& context, StorageBlock& sector) {
	auto& shell = context;
	if (!ensureStorageReady(context)) {
		return false;
	}

	if (!readRawStorageBlock0(context, sector)) {
		shell.writeLine("block0 read: failed");
		return false;
	}

	shell.writeLine("block0 read: ok");
	return true;
}

void printStorageBlock0Summary(lexxos::os::AppContext& shell, StorageBlock const& sector) {
	bool const bootSignature = sector[510] == 0x55 && sector[511] == 0xAA;
	shell.write("boot signature: ");
	shell.writeLine(bootSignature ? "55 AA" : "missing");

	std::uint16_t const bytesPerSector = readLe16(sector.data() + 11);
	std::uint8_t const sectorsPerCluster = sector[13];
	std::uint16_t const reservedSectors = readLe16(sector.data() + 14);
	std::uint8_t const fatCount = sector[16];
	if (bytesPerSector == 512 && sectorsPerCluster != 0 && reservedSectors != 0 && fatCount != 0) {
		shell.writeLine("layout: possible FAT boot sector at block 0");
		shell.write("sectors/cluster: ");
		writeUnsigned(shell, sectorsPerCluster);
		shell.writeLine("");
		shell.write("reserved sectors: ");
		writeUnsigned(shell, reservedSectors);
		shell.writeLine("");
		shell.write("fat count: ");
		writeUnsigned(shell, fatCount);
		shell.writeLine("");
	}

	if (bootSignature) {
		for (std::uint8_t index = 0; index < 4; ++index) {
			auto const* entry = sector.data() + 446 + index * 16;
			std::uint8_t const type = entry[4];
			std::uint32_t const firstBlock = readLe32(entry + 8);
			std::uint32_t const blockCount = readLe32(entry + 12);
			if (type == 0 && firstBlock == 0 && blockCount == 0) {
				continue;
			}

			shell.write("partition ");
			writeUnsigned(shell, index);
			shell.write(": type=");
			writeHex8(shell, type);
			shell.write(" first=");
			writeUnsigned(shell, firstBlock);
			shell.write(" count=");
			writeUnsigned(shell, blockCount);
			shell.writeLine("");
		}
	}

	shell.write("first 16 bytes:");
	for (std::size_t index = 0; index < 16; ++index) {
		shell.write(" ");
		writeHex8(shell, sector[index]);
	}
	shell.writeLine("");
}

bool resolvePath(lexxos::os::AppContext& context, std::string_view path, std::array<char, lexxos::vfs::maxPathLength>& output) {
	if (!lexxos::vfs::normalizePath(context.workingDirectory(), path, output.data(), output.size())) {
		context.writeLine("invalid path");
		return false;
	}
	return true;
}

void writePin(lexxos::os::AppContext& shell, lexxos::platform::GpioPin pin) {
	if (!pin.isValid()) {
		shell.write("n/a");
		return;
	}

	writeUnsigned(shell, static_cast<std::uint32_t>(pin.number));
}

void writePinValue(lexxos::os::AppContext& context, lexxos::platform::GpioPin pin) {
	if (!pin.isValid()) {
		context.write("-");
		return;
	}
	writeUnsigned(context, static_cast<std::uint32_t>(pin.number));
}

bool parseOptionalUnsignedArgument(
    lexxos::program::ParsedArguments const& arguments,
    std::string_view name,
    std::uint32_t& value) {
	auto const text = arguments.value(name);
	if (text.empty()) {
		return true;
	}
	return parseUnsigned32(text, value);
}

} // namespace lexxos::apps
