#include <lexxos/devices/sd_card.h>

#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/devices/storage/async_block_device.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_SD_BACKEND_SDIO)
	#include <lexxos/devices/storage/sd_sdio_backend.h>
#else
	#include <lexxos/devices/storage/sd_spi_backend.h>
#endif

namespace {

[[nodiscard]] lexxos::devices::SdCardInterface sdCardInterfaceFor(lexxos::platform::SdInterface interfaceType) {
	switch (interfaceType) {
		case lexxos::platform::SdInterface::Spi:
			return lexxos::devices::SdCardInterface::Spi;
		case lexxos::platform::SdInterface::Sdio1Bit:
		case lexxos::platform::SdInterface::Sdio4Bit:
			return lexxos::devices::SdCardInterface::Sdio;
		case lexxos::platform::SdInterface::None:
			break;
	}
	return lexxos::devices::SdCardInterface::Unknown;
}

[[nodiscard]] lexxos::platform::BoardDescriptor const& boardFor(lexxos::platform::PlatformServices const& platform) {
	auto const& board = platform.board;
	return board.current != nullptr ? board.current() : lexxos::platform::board();
}

void sleepMs(lexxos::platform::PlatformServices const& platform, std::uint32_t milliseconds) {
	auto const& clock = platform.clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

lexxos::devices::SdCardBackend& backend(lexxos::platform::PlatformServices const& platform) {
	auto const& board = boardFor(platform);
#if defined(LEXXOS_SD_BACKEND_SDIO)
	static lexxos::devices::SdSdioBackend storage{
	    board.sdSdio,
	    board.sdCardDetect,
	    platform,
	};
	return storage;
#else
	auto const sdSpiPins = lexxos::platform::effectiveSdSpi(board);

	static lexxos::devices::SdSpiBackend storage{
	    lexxos::devices::sdCardSpiBus(),
	    sdSpiPins,
	    board.sdCardDetect,
	    platform,
	};

	return storage;
#endif
}

lexxos::devices::SdCardBackend& backend() {
	return backend(lexxos::platform::services());
}

lexxos::devices::BlockDevice& exposedBlockDevice(lexxos::platform::PlatformServices const& platform) {
#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_KERNEL_FREERTOS)
	static lexxos::devices::AsyncBlockDevice asyncDevice{backend(platform), "sd-io", 6144, 3};
	return asyncDevice;
#else
	return backend(platform);
#endif
}

lexxos::devices::BlockDevice& exposedBlockDevice() {
	return exposedBlockDevice(lexxos::platform::services());
}

} // namespace

bool lexxos::devices::sd_card::initialize() {
	return initialize(lexxos::platform::services());
}

bool lexxos::devices::sd_card::initialize(lexxos::platform::PlatformServices const& platform) {
	return exposedBlockDevice(platform).initialize();
}

bool lexxos::devices::sd_card::probe(std::uint8_t attempts) {
	return probe(lexxos::platform::services(), attempts);
}

bool lexxos::devices::sd_card::probe(lexxos::platform::PlatformServices const& platform, std::uint8_t attempts) {
	if (attempts == 0) {
		attempts = 1;
	}
	for (std::uint8_t attempt = 0; attempt < attempts; ++attempt) {
		if (initialize(platform)) {
			return true;
		}
		if (attempt + 1 < attempts) {
			sleepMs(platform, static_cast<std::uint32_t>(100U + attempt * 100U));
		}
	}
	return false;
}

bool lexxos::devices::sd_card::isReady() {
	return backend().isReady();
}

lexxos::devices::SdCardInterface lexxos::devices::sd_card::interfaceType() {
	return backend().interfaceType();
}

lexxos::platform::SdInterface lexxos::devices::sd_card::boardInterface() {
	return boardInterface(lexxos::platform::services());
}

lexxos::platform::SdInterface lexxos::devices::sd_card::boardInterface(lexxos::platform::PlatformServices const& platform) {
	return lexxos::platform::effectiveSdInterface(boardFor(platform));
}

bool lexxos::devices::sd_card::backendMatchesBoardInterface() {
	return backendMatchesBoardInterface(lexxos::platform::services());
}

bool lexxos::devices::sd_card::backendMatchesBoardInterface(lexxos::platform::PlatformServices const& platform) {
	auto const expected = sdCardInterfaceFor(boardInterface(platform));
	return expected == SdCardInterface::Unknown || expected == backend(platform).interfaceType();
}

lexxos::devices::SdCardState lexxos::devices::sd_card::state() {
	return backend().state();
}

lexxos::devices::SdCardType lexxos::devices::sd_card::cardType() {
	return backend().cardType();
}

std::uint32_t lexxos::devices::sd_card::blockSizeBytes() {
	return backend().blockSizeBytes();
}

std::uint64_t lexxos::devices::sd_card::blockCount() {
	return backend().blockCount();
}

std::uint32_t lexxos::devices::sd_card::activeBaudRate() {
	return backend().activeClockHz();
}

lexxos::devices::SdTimingProfile lexxos::devices::sd_card::timingProfile() {
	return backend().timingProfile();
}

bool lexxos::devices::sd_card::setTransferBaudRate(std::uint32_t baudRate) {
	return backend().setTransferBaudRate(baudRate);
}

bool lexxos::devices::sd_card::setSpiClockMode(SpiClockMode mode) {
	return backend().setSpiClockMode(mode);
}

lexxos::devices::SpiClockMode lexxos::devices::sd_card::spiClockMode() {
	return backend().spiClockMode();
}

void lexxos::devices::sd_card::setReadCrcValidation(bool enabled) {
	backend().setReadCrcValidation(enabled);
}

bool lexxos::devices::sd_card::readCrcValidation() {
	return backend().readCrcValidation();
}

std::uint32_t lexxos::devices::sd_card::readRecoveryReinitializations() {
	return backend().readRecoveryReinitializations();
}

void lexxos::devices::sd_card::setMultiBlockRead(bool enabled) {
	backend().setMultiBlockRead(enabled);
}

bool lexxos::devices::sd_card::multiBlockReadEnabled() {
	return backend().multiBlockReadEnabled();
}

void lexxos::devices::sd_card::setMultiBlockWrite(bool enabled) {
	backend().setMultiBlockWrite(enabled);
}

bool lexxos::devices::sd_card::multiBlockWriteEnabled() {
	return backend().multiBlockWriteEnabled();
}

bool lexxos::devices::sd_card::dmaSupported() {
	return backend().dmaSupported();
}

void lexxos::devices::sd_card::setDmaTransfersEnabled(bool enabled) {
	backend().setDmaTransfersEnabled(enabled);
}

bool lexxos::devices::sd_card::dmaTransfersEnabled() {
	return backend().dmaTransfersEnabled();
}

void lexxos::devices::sd_card::setHighSpeedWritesEnabled(bool enabled) {
	backend().setHighSpeedWritesEnabled(enabled);
}

bool lexxos::devices::sd_card::highSpeedWritesEnabled() {
	return backend().highSpeedWritesEnabled();
}

void lexxos::devices::sd_card::setDiagnosticsEnabled(bool enabled) {
	backend().setDiagnosticsEnabled(enabled);
}

bool lexxos::devices::sd_card::diagnosticsEnabled() {
	return backend().diagnosticsEnabled();
}

void lexxos::devices::sd_card::clearDiagnostics() {
	backend().clearDiagnostics();
}

lexxos::devices::SdCardDiagnostics lexxos::devices::sd_card::diagnostics() {
	return backend().diagnostics();
}

bool lexxos::devices::sd_card::hasCardDetect() {
	return backend().hasCardDetect();
}

bool lexxos::devices::sd_card::isCardInserted() {
	return backend().isCardInserted();
}

lexxos::devices::BlockDevice* lexxos::devices::sd_card::blockDevice() {
	return &exposedBlockDevice();
}

lexxos::devices::BlockDevice* lexxos::devices::sd_card::blockDevice(lexxos::platform::PlatformServices const& platform) {
	return &exposedBlockDevice(platform);
}
