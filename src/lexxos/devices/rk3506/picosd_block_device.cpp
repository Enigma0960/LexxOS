#include <lexxos/devices/rk3506/picosd_block_device.h>

namespace lexxos::devices::rk3506 {

PicoSdBlockDevice::PicoSdBlockDevice(SdMediaTransport& transport, bool& writeArmed, void (*writeFailed)())
    : _transport(transport)
    , _writeArmed(writeArmed)
    , _writeFailed(writeFailed) {
}

PicoSdBlockDevice::PicoSdBlockDevice(SdMediaTransport& transport, bool& writeArmed, void (*writeFailed)(void*), void* context)
    : _transport(transport)
    , _writeArmed(writeArmed)
    , _contextWriteFailed(writeFailed)
    , _failureContext(context) {
}

bool PicoSdBlockDevice::initialize() {
	// Never reprobe a live/mounted card. The native transport enforces
	// its own mounted-media guard and never probes the boot SDMMC.
	if (isReady()) {
		return true;
	}

	return _transport.probe();
}

bool PicoSdBlockDevice::isReady() const {
	return _transport.ready() && _transport.sectorCount() != 0U;
}

bool PicoSdBlockDevice::isReadOnly() const {
	return !_writeArmed;
}

std::uint32_t PicoSdBlockDevice::blockSizeBytes() const {
	return 512U;
}

std::uint64_t PicoSdBlockDevice::blockCount() const {
	return isReady() ? _transport.sectorCount() : 0;
}

bool PicoSdBlockDevice::readBlocks(std::uint64_t first, void* destination, std::size_t count) {
	std::uint64_t const sectors = blockCount();
	if (destination == nullptr || count == 0U || first >= sectors || count > sectors - first) {
		return false;
	}

	auto* output = static_cast<std::uint8_t*>(destination);
	return _transport.read(static_cast<std::uint32_t>(first), output, count);
}

bool PicoSdBlockDevice::writeBlocks(std::uint64_t first, void const* source, std::size_t count) {
	std::uint64_t const sectors = blockCount();
	if (!_writeArmed || source == nullptr || count == 0U || first >= sectors || count > sectors - first) {
		return false;
	}

	if (!_transport.write(static_cast<std::uint32_t>(first),
	        static_cast<std::uint8_t const*>(source), count)) {
		// A partly committed FAT metadata write must never be retried
		// automatically. Require user inspection/reinsertion.
		_writeArmed = false;

		if (_contextWriteFailed) {
			_contextWriteFailed(_failureContext);
		} else if (_writeFailed) {
			_writeFailed();
		}

		return false;
	}

	return true;
}

SdCardInterface PicoSdBlockDevice::interfaceType() const {
	return SdCardInterface::Spi;
}

SdCardState PicoSdBlockDevice::state() const {
	if (!_transport.inserted()) {
		return SdCardState::NoCard;
	}

	if (isReady()) {
		return SdCardState::Ready;
	}

	return _transport.configured()
	    ? SdCardState::IoError
	    : SdCardState::Uninitialized;
}

SdCardType PicoSdBlockDevice::cardType() const {
	if (!isReady()) {
		return SdCardType::Unknown;
	}

	// The native diagnostic status distinguishes high-capacity cards.
	// It does not distinguish SDv1 from byte-addressed SDv2.
	return _transport.highCapacity()
	    ? SdCardType::SdV2BlockAddressed
	    : SdCardType::Unknown;
}

std::uint32_t PicoSdBlockDevice::activeClockHz() const {
	if (!isReady()) {
		return 0;
	}

	return _transport.clockMHz() * 1000000U;
}

SdTimingProfile PicoSdBlockDevice::timingProfile() const {
	return {
	    .initializationClockHz = 400000U,
	    .safeClockHz = 6000000U,
	    .defaultClockHz = 12000000U,
	    .performanceClockHz = 12000000U,
	    .maxClockHz = 12000000U,
	};
}

bool PicoSdBlockDevice::setTransferBaudRate(std::uint32_t baudRate) {
	if (!isReady() || (baudRate != 6000000U && baudRate != 12000000U)) {
		return false;
	}

	// The native controller accepts only its validated 6/12 MHz modes.
	// It refuses changes while SPI1 is busy.
	return _transport.setClockMHz(
	    baudRate / 1000000U);
}

bool PicoSdBlockDevice::setSpiClockMode(SpiClockMode mode) {
	// Only mode 0 is supported by the RK3506 production driver.
	return mode == SpiClockMode::Mode0;
}

SpiClockMode PicoSdBlockDevice::spiClockMode() const {
	return SpiClockMode::Mode0;
}

bool PicoSdBlockDevice::readCrcValidation() const {
	// The native transport always checks received CRC16.
	// The generic request to disable verification remains unsupported.
	return true;
}

bool PicoSdBlockDevice::multiBlockReadEnabled() const {
	// Native readSectors() uses CMD18 for batches larger than one.
	return true;
}

bool PicoSdBlockDevice::dmaSupported() const {
	return _transport.dmaAvailable();
}

bool PicoSdBlockDevice::dmaTransfersEnabled() const {
	return _transport.dmaEnabled();
}

bool PicoSdBlockDevice::hasCardDetect() const {
	return true;
}

bool PicoSdBlockDevice::isCardInserted() const {
	return _transport.inserted();
}

} // namespace lexxos::devices::rk3506
