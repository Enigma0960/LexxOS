#include <lexxos/devices/storage/sd_spi_backend.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include <lexxos/devices/storage/detail/sd_card_transfer_guard.h>
#include <lexxos/devices/storage/detail/sd_spi_backend_detail.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {
using storage::detail::acmd41SendOpCond;
using storage::detail::cmd0Attempts;
using storage::detail::cmd0GoIdle;
using storage::detail::cmd13SendStatus;
using storage::detail::cmd16SetBlockLength;
using storage::detail::cmd17ReadSingleBlock;
using storage::detail::cmd18ReadMultipleBlock;
using storage::detail::cmd24WriteSingleBlock;
using storage::detail::cmd25WriteMultipleBlock;
using storage::detail::cmd55ApplicationCommand;
using storage::detail::cmd58ReadOcr;
using storage::detail::cmd8SendIfCond;
using storage::detail::cmd9SendCsd;
using storage::detail::crc16Ccitt;
using storage::detail::defaultHighSpeedWritesEnabled;
using storage::detail::defaultMultiBlockReadEnabled;
using storage::detail::defaultMultiBlockWriteEnabled;
using storage::detail::defaultReadCrcValidation;
using storage::detail::diagnosticCodeFor;
using storage::detail::hostCapacitySupport;
using storage::detail::r1Idle;
using storage::detail::r1IllegalCommand;
using storage::detail::readRecoveryAttempts;
using storage::detail::recoveryDelayMs;
using storage::detail::reinitializeOnReadFailure;
using storage::detail::SdCardTransferGuard;
using storage::detail::sdPowerUpDelayMs;
using storage::detail::transferRecoveryBaudRate;
using storage::detail::u32FromBytes;
using storage::detail::writeRecoveryAttempts;

SdSpiBackend::SdSpiBackend(SpiBus& bus, lexxos::platform::SpiPins pins, lexxos::platform::GpioCardDetect cardDetect)
    : SdSpiBackend(bus, pins, cardDetect, lexxos::platform::services()) {
}

SdSpiBackend::SdSpiBackend(
    SpiBus& bus,
    lexxos::platform::SpiPins pins,
    lexxos::platform::GpioCardDetect cardDetect,
    lexxos::platform::PlatformServices const& platform)
    : _platformServices(platform)
    , _transport(bus, pins)
    , _protocol(_transport)
    , _blockOps(_transport, _protocol)
    , _cardDetect(cardDetect) {
}

bool SdSpiBackend::initialize() {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		recordDiagnostic(SdCardDiagnosticCode::LockFailed);
		return false;
	}
	bool const ok = initializeCard();
	if (!ok) {
		recordDiagnostic(SdCardDiagnosticCode::InitializeFailed);
	}
	return ok;
}

bool SdSpiBackend::initializeCard() {
	_state = SdCardState::Uninitialized;
	_cardType = SdCardType::Unknown;
	_blockCount = 0;
	_activeBaudRate = 0;
	_validateReadCrc = defaultReadCrcValidation;
	_multiBlockReadEnabled = defaultMultiBlockReadEnabled;
	_multiBlockWriteEnabled = defaultMultiBlockWriteEnabled;
	_highSpeedWritesEnabled = defaultHighSpeedWritesEnabled;

	if (!configureCardDetect()) {
		setFailure(SdCardState::NoBus);
		return false;
	}
	if (!physicalCardPresent()) {
		setFailure(SdCardState::NoCard);
		return false;
	}

	if (!_transport.initialize()) {
		setFailure(SdCardState::NoBus);
		return false;
	}

	if (!_transport.setClockMode(SpiClockMode::Mode0)) {
		setFailure(SdCardState::IoError);
		return false;
	}

	_transport.deselect();
	if (!setActiveBaudRate(initializationBaudRate)) {
		setFailure(SdCardState::IoError);
		return false;
	}

	// SD cards need at least 74 clocks with CS high before the first command.
	// In practice, hot-plug modules and long wires are often slower, so keep
	// the reset sequence intentionally conservative.
	sleepMs(sdPowerUpDelayMs);
	if (!clockIdle(32)) {
		setFailure(SdCardState::IoError);
		return false;
	}

	std::uint8_t r1 = commandTimeoutResponse;
	for (std::uint8_t attempt = 0; attempt < cmd0Attempts; ++attempt) {
		r1 = sendCommand(cmd0GoIdle, 0, 0x95);
		if (r1 == r1Idle) {
			break;
		}

		_transport.deselect();
		clockIdle(4);
		sleepMs(2);
	}

	if (r1 != r1Idle) {
		setFailure(r1 == commandTimeoutResponse ? classifyMissingCardState() : SdCardState::IoError);
		return false;
	}

	std::array<std::uint8_t, 4> cmd8Response{};
	r1 = sendCommand(cmd8SendIfCond, 0x000001AA, 0x87, cmd8Response.data(), cmd8Response.size(), false);
	bool const sdV2 = (r1 & r1IllegalCommand) == 0;
	if (sdV2) {
		std::uint32_t const echo = u32FromBytes(cmd8Response.data());
		if ((echo & 0x00000FFFU) != 0x000001AAU) {
			setFailure(SdCardState::Unsupported);
			return false;
		}
	}

	bool enteredReadyState = false;
	for (std::uint16_t attempt = 0; attempt < 1000; ++attempt) {
		r1 = sendApplicationCommand(acmd41SendOpCond, sdV2 ? hostCapacitySupport : 0);
		if (r1 == 0x00) {
			enteredReadyState = true;
			break;
		}
		if (r1 == commandTimeoutResponse) {
			break;
		}
		sleepMs(1);
	}

	if (!enteredReadyState) {
		setFailure(r1 == commandTimeoutResponse ? SdCardState::Timeout : SdCardState::IoError);
		return false;
	}

	if (sdV2) {
		std::array<std::uint8_t, 4> ocr{};
		r1 = sendCommand(cmd58ReadOcr, 0, 0x01, ocr.data(), ocr.size(), false);
		if (r1 != 0x00) {
			setFailure(SdCardState::IoError);
			return false;
		}
		_cardType = (ocr[0] & 0x40U) != 0 ? SdCardType::SdV2BlockAddressed : SdCardType::SdV2ByteAddressed;
	} else {
		_cardType = SdCardType::SdV1;
	}

	if (_cardType != SdCardType::SdV2BlockAddressed) {
		r1 = sendCommand(cmd16SetBlockLength, blockSize, 0x01);
		if (r1 != 0x00) {
			setFailure(SdCardState::Unsupported);
			return false;
		}
	}

	if (!readCsd()) {
		setFailure(SdCardState::IoError);
		return false;
	}

	if (_transport.configuredBaudRate() > initializationBaudRate && !setActiveBaudRate(_transport.configuredBaudRate())) {
		setFailure(SdCardState::IoError);
		return false;
	}

	_state = SdCardState::Ready;
	return true;
}

bool SdSpiBackend::isReady() const {
	return _state == SdCardState::Ready && physicalCardPresent();
}

std::uint32_t SdSpiBackend::blockSizeBytes() const {
	return blockSize;
}

std::uint64_t SdSpiBackend::blockCount() const {
	return isReady() ? _blockCount : 0;
}

bool SdSpiBackend::readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t blockCount) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		recordDiagnostic(SdCardDiagnosticCode::LockFailed, 0xFFU, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		return false;
	}

	if (buffer == nullptr) {
		recordDiagnostic(SdCardDiagnosticCode::NullBuffer, 0xFFU, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		return false;
	}
	if (!isReady()) {
		if (!physicalCardPresent()) {
			recordDiagnostic(SdCardDiagnosticCode::CardRemoved, 0xFFU, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
			setFailure(SdCardState::CardRemoved);
		} else {
			recordDiagnostic(SdCardDiagnosticCode::NotReady, 0xFFU, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		}
		return false;
	}

	if (blockCount == 0) {
		return true;
	}

	auto* output = static_cast<std::uint8_t*>(buffer);
	if (blockCount > 1 && _multiBlockReadEnabled) {
		if (readMultipleBlocksWithRecovery(firstBlock, output, blockCount)) {
			return true;
		}
		return false;
	}

	for (std::size_t index = 0; index < blockCount; ++index) {
		if (!readSingleBlockWithRecovery(firstBlock + index, output + index * blockSize)) {
			return false;
		}
	}
	return true;
}

bool SdSpiBackend::writeBlocks(std::uint64_t firstBlock, void const* buffer, std::size_t blockCount) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		recordDiagnostic(SdCardDiagnosticCode::LockFailed, 0xFFU, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		return false;
	}

	if (buffer == nullptr) {
		recordDiagnostic(SdCardDiagnosticCode::NullBuffer, 0xFFU, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		return false;
	}
	if (!isReady()) {
		if (!physicalCardPresent()) {
			recordDiagnostic(SdCardDiagnosticCode::CardRemoved, 0xFFU, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
			setFailure(SdCardState::CardRemoved);
		} else {
			recordDiagnostic(SdCardDiagnosticCode::NotReady, 0xFFU, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		}
		return false;
	}

	if (blockCount == 0) {
		return true;
	}

	if (!writeTransferAllowed()) {
		recordDiagnostic(SdCardDiagnosticCode::WriteSpeedGuard, 0xFFU, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		return false;
	}

	auto const* input = static_cast<std::uint8_t const*>(buffer);
	if (blockCount > 1 && _multiBlockWriteEnabled) {
		if (writeMultipleBlocksWithRecovery(firstBlock, input, blockCount)) {
			return true;
		}
		return false;
	}

	for (std::size_t index = 0; index < blockCount; ++index) {
		if (!writeSingleBlockWithRecovery(firstBlock + index, input + index * blockSize)) {
			setFailure(SdCardState::IoError);
			return false;
		}
	}
	return true;
}

SdCardInterface SdSpiBackend::interfaceType() const {
	return SdCardInterface::Spi;
}

SdCardState SdSpiBackend::state() const {
	if (_state == SdCardState::Ready && !physicalCardPresent()) {
		return SdCardState::CardRemoved;
	}
	return _state;
}

SdCardType SdSpiBackend::cardType() const {
	return _cardType;
}

std::uint32_t SdSpiBackend::activeBaudRate() const {
	return _activeBaudRate;
}

std::uint32_t SdSpiBackend::activeClockHz() const {
	return activeBaudRate();
}

SdTimingProfile SdSpiBackend::timingProfile() const {
	return {
	    .initializationClockHz = initializationBaudRate,
	    .safeClockHz = safeTransferBaudRate,
	    .defaultClockHz = defaultTransferBaudRate,
	    .performanceClockHz = performanceTransferBaudRate,
	    .maxClockHz = maxTransferBaudRate,
	};
}

bool SdSpiBackend::setSpiClockMode(SpiClockMode mode) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		recordDiagnostic(SdCardDiagnosticCode::LockFailed);
		return false;
	}

	if (!_transport.setClockMode(mode)) {
		recordDiagnostic(SdCardDiagnosticCode::SetModeFailed, 0xFFU, static_cast<std::uint32_t>(mode));
		return false;
	}
	if (_transport.isReady()) {
		_transport.deselect();
		(void) clockIdle(2);
	}
	return true;
}

SpiClockMode SdSpiBackend::spiClockMode() const {
	return _transport.clockMode();
}

void SdSpiBackend::setReadCrcValidation(bool enabled) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		return;
	}

	_validateReadCrc = enabled;
}

bool SdSpiBackend::readCrcValidation() const {
	return _validateReadCrc;
}

void SdSpiBackend::setMultiBlockRead(bool enabled) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		return;
	}

	_multiBlockReadEnabled = enabled;
}

bool SdSpiBackend::multiBlockReadEnabled() const {
	return _multiBlockReadEnabled;
}

void SdSpiBackend::setMultiBlockWrite(bool enabled) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		recordDiagnostic(SdCardDiagnosticCode::LockFailed);
		return;
	}

	_multiBlockWriteEnabled = enabled;
}

bool SdSpiBackend::multiBlockWriteEnabled() const {
	return _multiBlockWriteEnabled;
}

bool SdSpiBackend::dmaSupported() const {
	return _transport.supportsDma();
}

void SdSpiBackend::setDmaTransfersEnabled(bool enabled) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		recordDiagnostic(SdCardDiagnosticCode::LockFailed);
		return;
	}

	_dmaTransfersEnabled = enabled;
}

bool SdSpiBackend::dmaTransfersEnabled() const {
	return _dmaTransfersEnabled;
}

void SdSpiBackend::setHighSpeedWritesEnabled(bool enabled) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		recordDiagnostic(SdCardDiagnosticCode::LockFailed);
		return;
	}

	_highSpeedWritesEnabled = enabled;
}

bool SdSpiBackend::highSpeedWritesEnabled() const {
	return _highSpeedWritesEnabled;
}

void SdSpiBackend::setDiagnosticsEnabled(bool enabled) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		return;
	}

	_diagnosticsEnabled = enabled;
}

bool SdSpiBackend::diagnosticsEnabled() const {
	return _diagnosticsEnabled;
}

void SdSpiBackend::clearDiagnostics() {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		return;
	}

	_diagnosticSequence = 0;
	_diagnosticWriteIndex = 0;
	_diagnosticStoredCount = 0;
	_diagnosticDroppedCount = 0;
	_diagnosticEntries = {};
}

SdCardDiagnostics SdSpiBackend::diagnostics() {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		SdCardDiagnostics snapshot{};
		snapshot.enabled = _diagnosticsEnabled;
		return snapshot;
	}

	return diagnosticsUnlocked();
}

std::uint32_t SdSpiBackend::readRecoveryReinitializations() const {
	return _readRecoveryReinitializations;
}

bool SdSpiBackend::setTransferBaudRate(std::uint32_t baudRate) {
	SdCardTransferGuard guard;
	if (!guard.locked()) {
		recordDiagnostic(SdCardDiagnosticCode::LockFailed);
		return false;
	}

	if (baudRate < initializationBaudRate || !isReady()) {
		recordDiagnostic(SdCardDiagnosticCode::SetBaudFailed, 0xFFU, baudRate);
		return false;
	}

	if (baudRate > maxTransferBaudRate) {
		recordDiagnostic(SdCardDiagnosticCode::SetBaudFailed, 0xFFU, baudRate);
		return false;
	}

	bool const ok = setActiveBaudRate(baudRate);
	if (!ok) {
		recordDiagnostic(SdCardDiagnosticCode::SetBaudFailed, 0xFFU, baudRate);
	}
	return ok;
}

bool SdSpiBackend::hasCardDetect() const {
	return _cardDetect.isValid();
}

bool SdSpiBackend::isCardInserted() const {
	return physicalCardPresent();
}

bool SdSpiBackend::clockIdle(std::size_t byteCount) {
	return _protocol.clockIdle(byteCount);
}

bool SdSpiBackend::readByte(std::uint8_t& value) {
	return _protocol.readByte(value);
}

bool SdSpiBackend::writeByte(std::uint8_t value) {
	return _protocol.writeByte(value);
}

bool SdSpiBackend::waitReady(std::uint32_t attempts) {
	return _protocol.waitReady(attempts);
}

bool SdSpiBackend::waitDataToken(std::uint32_t attempts) {
	return _protocol.waitDataToken(attempts);
}

std::uint8_t SdSpiBackend::sendCommand(
    std::uint8_t command,
    std::uint32_t argument,
    std::uint8_t crc,
    std::uint8_t* response,
    std::size_t responseSize,
    bool keepSelected) {
	SdSpiCommandResult const result = _protocol.sendCommand(command, argument, crc, response, responseSize, keepSelected);
	if (result.error != SdSpiProtocolError::None) {
		recordDiagnostic(diagnosticCodeFor(result.error), command, result.diagnosticR1);
	}

	return result.r1;
}

std::uint8_t SdSpiBackend::sendCommand(std::uint8_t command, std::uint32_t argument, std::uint8_t crc, bool keepSelected) {
	return sendCommand(command, argument, crc, nullptr, 0, keepSelected);
}

std::uint8_t SdSpiBackend::sendApplicationCommand(std::uint8_t command, std::uint32_t argument) {
	std::uint8_t const r1 = sendCommand(cmd55ApplicationCommand, 0, 0x01);
	if (r1 > r1Idle) {
		return r1;
	}
	return sendCommand(command, argument, 0x01);
}

bool SdSpiBackend::readRegister(std::uint8_t command, std::array<std::uint8_t, 16>& registerData) {
	std::uint8_t const r1 = sendCommand(command, 0, 0x01, true);
	if (r1 != 0x00) {
		endTransaction();
		return false;
	}

	if (!waitDataToken(100000)) {
		recordDiagnostic(SdCardDiagnosticCode::RegisterTokenTimeout, command, r1);
		endTransaction();
		return false;
	}

	std::array<std::uint8_t, 2> crc{};
	bool const dataOk = _transport.read(0xFF, registerData.data(), registerData.size()) && _transport.read(0xFF, crc.data(), crc.size());
	bool const ok = dataOk && validateReadCrc(registerData.data(), registerData.size(), crc);
	if (!ok) {
		recordDiagnostic(dataOk ? SdCardDiagnosticCode::ReadCrcMismatch : SdCardDiagnosticCode::RegisterReadFailed, command, r1);
	}
	endTransaction();
	return ok;
}

bool SdSpiBackend::readSingleBlock(std::uint64_t block, std::uint8_t* buffer) {
	SdSpiBlockResult const result = _blockOps.readSingleBlock(commandAddress(block), block, buffer, blockTransferConfig());
	if (!result.ok) {
		recordBlockDiagnostic(result);
	}

	return result.ok;
}

bool SdSpiBackend::readSingleBlockWithRecovery(std::uint64_t block, std::uint8_t* buffer) {
	for (std::uint8_t attempt = 0; attempt < readRecoveryAttempts; ++attempt) {
		if (readSingleBlock(block, buffer)) {
			return true;
		}
		if (!physicalCardPresent()) {
			setFailure(_cardDetect.isValid() ? SdCardState::CardRemoved : SdCardState::Timeout);
			return false;
		}

		// High SPI speeds may be marginal with long wires or SD modules with level
		// shifters.  Retrying every sector once at the requested high speed and
		// then falling back to init speed makes benchmarks catastrophically slow.
		// Drop to a conservative transfer speed and keep it until the next explicit
		// card initialization; this turns a marginal high-speed configuration into
		// a stable, predictable mode instead of one timeout per block.
		std::uint32_t const fallbackBaud = std::min(_transport.configuredBaudRate(), transferRecoveryBaudRate);
		if (_transport.configuredBaudRate() > initializationBaudRate && _activeBaudRate != fallbackBaud) {
			recordDiagnostic(SdCardDiagnosticCode::ReadFallbackToSafeBaud, cmd17ReadSingleBlock, 0xFFFFFFFFU, block, 1);
			setActiveBaudRate(fallbackBaud);
		}
		clockIdle(2);
		clockIdle(8);
		sleepMs(recoveryDelayMs);
	}
	return recoverReadPath(block, buffer);
}

bool SdSpiBackend::recoverReadPath(std::uint64_t block, std::uint8_t* buffer) {
	if (!reinitializeOnReadFailure) {
		markTransferFailure();
		return false;
	}
	if (!physicalCardPresent()) {
		setFailure(_cardDetect.isValid() ? SdCardState::CardRemoved : SdCardState::Timeout);
		return false;
	}
	++_readRecoveryReinitializations;
	_transport.deselect();
	(void) clockIdle(16);
	sleepMs(recoveryDelayMs);
	// A failed CMD17 after a long ROM stream can leave some cards or
	// level-shifter based modules in a state where the next FAT metadata sector
	// is no longer readable, while the high-level state still says "ready".
	// Re-run card initialization once and retry the original block before
	// reporting a disk error to FatFs.
	if (!initializeCard()) {
		recordDiagnostic(SdCardDiagnosticCode::ReinitializeFailed, cmd17ReadSingleBlock, 0xFFFFFFFFU, block, 1);
		return false;
	}
	for (std::uint8_t attempt = 0; attempt < readRecoveryAttempts; ++attempt) {
		if (readSingleBlock(block, buffer)) {
			return true;
		}
		clockIdle(8);
		sleepMs(recoveryDelayMs);
	}
	markTransferFailure();
	return false;
}

bool SdSpiBackend::readMultipleBlocks(std::uint64_t firstBlock, std::uint8_t* buffer, std::size_t blockCount) {
	SdSpiBlockResult const result = _blockOps.readMultipleBlocks(commandAddress(firstBlock), firstBlock, buffer, blockCount, blockTransferConfig());
	if (!result.ok) {
		recordBlockDiagnostic(result);
	}
	return result.ok;
}

bool SdSpiBackend::readMultipleBlocksWithRecovery(std::uint64_t firstBlock, std::uint8_t* buffer, std::size_t blockCount) {
	for (std::uint8_t attempt = 0; attempt < readRecoveryAttempts; ++attempt) {
		if (readMultipleBlocks(firstBlock, buffer, blockCount)) {
			return true;
		}
		if (!physicalCardPresent()) {
			setFailure(_cardDetect.isValid() ? SdCardState::CardRemoved : SdCardState::Timeout);
			return false;
		}

		std::uint32_t const fallbackBaud = std::min(_transport.configuredBaudRate(), transferRecoveryBaudRate);
		if (_transport.configuredBaudRate() > initializationBaudRate && _activeBaudRate != fallbackBaud) {
			recordDiagnostic(SdCardDiagnosticCode::ReadFallbackToSafeBaud, cmd18ReadMultipleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
			setActiveBaudRate(fallbackBaud);
		}
		clockIdle(2);
		clockIdle(8);
		sleepMs(recoveryDelayMs);
	}

	// Fall back to the hardened single-block path.  It can perform a full card
	// reinitialization on the first unrecoverable block and keeps the caller's
	// buffer layout identical to a successful multi-block transfer.
	for (std::size_t index = 0; index < blockCount; ++index) {
		if (!readSingleBlockWithRecovery(firstBlock + index, buffer + index * blockSize)) {
			return false;
		}
	}
	return true;
}

bool SdSpiBackend::writeSingleBlock(std::uint64_t block, std::uint8_t const* buffer) {
	SdSpiBlockResult const result = _blockOps.writeSingleBlock(commandAddress(block), block, buffer, blockTransferConfig());
	if (!result.ok) {
		recordBlockDiagnostic(result);
	}
	return result.ok;
}

bool SdSpiBackend::writeMultipleBlocks(std::uint64_t firstBlock, std::uint8_t const* buffer, std::size_t blockCount) {
	SdSpiBlockResult const result = _blockOps.writeMultipleBlocks(commandAddress(firstBlock), firstBlock, buffer, blockCount, blockTransferConfig());
	if (!result.ok) {
		recordBlockDiagnostic(result);
	}
	return result.ok;
}

bool SdSpiBackend::writeTransferAllowed() const {
	return _transport.clockMode() == SpiClockMode::Mode0 && (_activeBaudRate <= safeWriteBaudRate || _highSpeedWritesEnabled);
}

bool SdSpiBackend::writeSingleBlockWithRecovery(std::uint64_t block, std::uint8_t const* buffer) {
	for (std::uint8_t attempt = 0; attempt < writeRecoveryAttempts; ++attempt) {
		if (writeSingleBlock(block, buffer)) {
			return true;
		}

		std::uint32_t const fallbackBaud = std::min(_transport.configuredBaudRate(), transferRecoveryBaudRate);
		if (_transport.configuredBaudRate() > initializationBaudRate && _activeBaudRate != fallbackBaud) {
			recordDiagnostic(SdCardDiagnosticCode::WriteFallbackToSafeBaud, cmd24WriteSingleBlock, 0xFFFFFFFFU, block, 1);
			setActiveBaudRate(fallbackBaud);
		}
		clockIdle(2);
		sleepMs(recoveryDelayMs);
	}

	markTransferFailure();
	return false;
}

bool SdSpiBackend::writeMultipleBlocksWithRecovery(std::uint64_t firstBlock, std::uint8_t const* buffer, std::size_t blockCount) {
	for (std::uint8_t attempt = 0; attempt < writeRecoveryAttempts; ++attempt) {
		if (writeMultipleBlocks(firstBlock, buffer, blockCount)) {
			return true;
		}

		std::uint32_t const fallbackBaud = std::min(_transport.configuredBaudRate(), transferRecoveryBaudRate);
		if (_transport.configuredBaudRate() > initializationBaudRate && _activeBaudRate != fallbackBaud) {
			recordDiagnostic(SdCardDiagnosticCode::WriteFallbackToSafeBaud, cmd25WriteMultipleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
			setActiveBaudRate(fallbackBaud);
		}
		clockIdle(2);
		sleepMs(recoveryDelayMs);
	}

	markTransferFailure();
	return false;
}

bool SdSpiBackend::readCsd() {
	std::array<std::uint8_t, 16> csd{};
	if (!readRegister(cmd9SendCsd, csd)) {
		return false;
	}

	std::uint8_t const csdStructure = static_cast<std::uint8_t>((csd[0] >> 6) & 0x03U);
	if (csdStructure == 1) {
		std::uint32_t const cSize = (static_cast<std::uint32_t>(csd[7] & 0x3FU) << 16)
		    | (static_cast<std::uint32_t>(csd[8]) << 8)
		    | static_cast<std::uint32_t>(csd[9]);
		_blockCount = (static_cast<std::uint64_t>(cSize) + 1U) * 1024U;
		return _blockCount > 0;
	}

	if (csdStructure == 0) {
		std::uint32_t const readBlockLength = static_cast<std::uint32_t>(csd[5] & 0x0FU);
		std::uint32_t const cSize = (static_cast<std::uint32_t>(csd[6] & 0x03U) << 10)
		    | (static_cast<std::uint32_t>(csd[7]) << 2)
		    | static_cast<std::uint32_t>((csd[8] & 0xC0U) >> 6);
		std::uint32_t const cSizeMultiplier = (static_cast<std::uint32_t>(csd[9] & 0x03U) << 1)
		    | static_cast<std::uint32_t>((csd[10] & 0x80U) >> 7);
		std::uint64_t const blockLengthBytes = 1ULL << readBlockLength;
		std::uint64_t const blockNumber = (static_cast<std::uint64_t>(cSize) + 1ULL) << (cSizeMultiplier + 2U);
		std::uint64_t const capacityBytes = blockNumber * blockLengthBytes;
		_blockCount = capacityBytes / blockSize;
		return _blockCount > 0;
	}

	return false;
}

bool SdSpiBackend::configureCardDetect() {
	if (!_cardDetect.isValid()) {
		return true;
	}

	lexxos::platform::GpioPull const pull = _cardDetect.insertedLevel == lexxos::platform::GpioActiveLevel::Low
	    ? lexxos::platform::GpioPull::Up
	    : lexxos::platform::GpioPull::Down;
	auto const& gpio = _platformServices.gpio;
	return gpio.configureInput != nullptr && gpio.configureInput(_cardDetect.detect, pull);
}

bool SdSpiBackend::physicalCardPresent() const {
	if (!_cardDetect.isValid()) {
		return true;
	}

	bool value = false;
	auto const& gpio = _platformServices.gpio;
	if (gpio.read == nullptr || !gpio.read(_cardDetect.detect, value)) {
		return false;
	}

	return _cardDetect.insertedLevel == lexxos::platform::GpioActiveLevel::High ? value : !value;
}

bool SdSpiBackend::setActiveBaudRate(std::uint32_t baudRate) {
	if (!_transport.setBaudRate(baudRate)) {
		recordDiagnostic(SdCardDiagnosticCode::SetBaudFailed, 0xFFU, baudRate);
		return false;
	}

	_activeBaudRate = baudRate;
	_transport.deselect();
	(void) clockIdle(2);
	return true;
}

void SdSpiBackend::markTransferFailure() {
	recordDiagnostic(SdCardDiagnosticCode::TransferFailure);
	setFailure(classifyCardAfterFailure());
}

void SdSpiBackend::recordDiagnostic(
    SdCardDiagnosticCode code,
    std::uint32_t command,
    std::uint32_t r1,
    std::uint64_t block,
    std::uint32_t blockCount) {
	if (!_diagnosticsEnabled) {
		++_diagnosticDroppedCount;
		return;
	}

	SdCardDiagnosticEntry entry{};
	entry.sequence = ++_diagnosticSequence;
	entry.uptimeMs = uptimeMs();
	entry.code = code;
	entry.state = _state;
	entry.cardType = _cardType;
	entry.mode = _transport.clockMode();
	entry.activeBaudRate = _activeBaudRate;
	entry.command = command;
	entry.r1 = r1;
	entry.block = block;
	entry.blockCount = blockCount;

	_diagnosticEntries[_diagnosticWriteIndex % sdCardDiagnosticEntryCount] = entry;
	++_diagnosticWriteIndex;
	if (_diagnosticStoredCount < sdCardDiagnosticEntryCount) {
		++_diagnosticStoredCount;
	} else {
		++_diagnosticDroppedCount;
	}
}

SdCardDiagnostics SdSpiBackend::diagnosticsUnlocked() const {
	SdCardDiagnostics snapshot{};
	snapshot.enabled = _diagnosticsEnabled;
	snapshot.sequence = _diagnosticSequence;
	snapshot.storedCount = _diagnosticStoredCount;
	snapshot.droppedCount = _diagnosticDroppedCount;

	std::uint32_t const first = _diagnosticStoredCount == sdCardDiagnosticEntryCount
	    ? (_diagnosticWriteIndex % sdCardDiagnosticEntryCount)
	    : 0;
	for (std::uint32_t index = 0; index < _diagnosticStoredCount; ++index) {
		snapshot.entries[index] = _diagnosticEntries[(first + index) % sdCardDiagnosticEntryCount];
	}
	return snapshot;
}

SdCardState SdSpiBackend::classifyMissingCardState() const {
	if (_cardDetect.isValid()) {
		return physicalCardPresent() ? SdCardState::Timeout : SdCardState::NoCard;
	}
	return SdCardState::Timeout;
}

SdCardState SdSpiBackend::classifyCardAfterFailure() {
	if (!physicalCardPresent()) {
		return _cardDetect.isValid() ? SdCardState::CardRemoved : SdCardState::Timeout;
	}

	if (_activeBaudRate != initializationBaudRate) {
		setActiveBaudRate(initializationBaudRate);
	}

	clockIdle(4);
	std::array<std::uint8_t, 2> status{};
	std::uint8_t const r1 = sendCommand(cmd13SendStatus, 0, 0x01, status.data(), status.size(), false);
	return r1 == commandTimeoutResponse ? classifyMissingCardState() : SdCardState::IoError;
}

std::uint32_t SdSpiBackend::uptimeMs() const {
	auto const& clock = _platformServices.clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

void SdSpiBackend::sleepMs(std::uint32_t milliseconds) const {
	auto const& clock = _platformServices.clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

bool SdSpiBackend::validateReadCrc(std::uint8_t const* data, std::size_t size, std::array<std::uint8_t, 2> const& crc) const {
	if (!_validateReadCrc) {
		return true;
	}
	std::uint16_t const expected = static_cast<std::uint16_t>((static_cast<std::uint16_t>(crc[0]) << 8) | crc[1]);
	return crc16Ccitt(data, size) == expected;
}

SdSpiBlockTransferConfig SdSpiBackend::blockTransferConfig() const {
	return {
	    .useDma = _dmaTransfersEnabled,
	    .validateCrc = _validateReadCrc,
	    .dmaTimeoutMs = dmaTransferTimeoutMs,
	};
}

void SdSpiBackend::recordBlockDiagnostic(SdSpiBlockResult const& result) {
	if (!result.ok && result.code != SdCardDiagnosticCode::None) {
		recordDiagnostic(result.code, result.command, result.r1, result.block, result.blockCount);
	}
}

void SdSpiBackend::endTransaction() {
	_protocol.endTransaction();
}

std::uint32_t SdSpiBackend::commandAddress(std::uint64_t block) const {
	std::uint64_t const address = _cardType == SdCardType::SdV2BlockAddressed ? block : block * blockSize;
	return static_cast<std::uint32_t>(address & 0xFFFFFFFFULL);
}

void SdSpiBackend::setFailure(SdCardState state) {
	_state = state;
	_cardType = SdCardType::Unknown;
	_blockCount = 0;
	_activeBaudRate = 0;
	_validateReadCrc = false;
	_multiBlockReadEnabled = false;
	_multiBlockWriteEnabled = false;
	_highSpeedWritesEnabled = false;
	if (_transport.isReady()) {
		(void) _transport.setClockMode(SpiClockMode::Mode0);
		(void) _transport.setBaudRate(initializationBaudRate);
	}
	_transport.deselect();
}

} // namespace lexxos::devices
