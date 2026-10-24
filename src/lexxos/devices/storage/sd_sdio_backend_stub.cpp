#include <lexxos/devices/storage/sd_sdio_backend.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

SdSdioBackend::SdSdioBackend(lexxos::platform::SdioPins pins, lexxos::platform::GpioCardDetect cardDetect)
    : SdSdioBackend(pins, cardDetect, lexxos::platform::services()) {
}

SdSdioBackend::SdSdioBackend(
    lexxos::platform::SdioPins pins,
    lexxos::platform::GpioCardDetect cardDetect,
    lexxos::platform::PlatformServices const& platform)
    : _platformServices(platform)
    , _pins(pins)
    , _cardDetect(cardDetect) {
}

bool SdSdioBackend::initialize() {
	resetState();

	if (!_pins.isValid()) {
		recordDiagnostic(SdCardDiagnosticCode::InitializeFailed);
		setFailure(SdCardState::NoBus);
		return false;
	}
	if (!physicalCardPresent()) {
		recordDiagnostic(SdCardDiagnosticCode::CardRemoved);
		setFailure(SdCardState::NoCard);
		return false;
	}

	recordDiagnostic(SdCardDiagnosticCode::InitializeFailed);
	setFailure(SdCardState::Unsupported);
	return false;
}

bool SdSdioBackend::isReady() const {
	return false;
}

std::uint32_t SdSdioBackend::blockSizeBytes() const {
	return blockSize;
}

std::uint64_t SdSdioBackend::blockCount() const {
	return 0;
}

bool SdSdioBackend::readBlocks(std::uint64_t, void*, std::size_t) {
	recordDiagnostic(SdCardDiagnosticCode::NotReady);
	setFailure(SdCardState::Unsupported);
	return false;
}

bool SdSdioBackend::writeBlocks(std::uint64_t, void const*, std::size_t) {
	recordDiagnostic(SdCardDiagnosticCode::NotReady);
	setFailure(SdCardState::Unsupported);
	return false;
}

SdCardInterface SdSdioBackend::interfaceType() const {
	return SdCardInterface::Sdio;
}

SdCardState SdSdioBackend::state() const {
	if (_state == SdCardState::Ready && !physicalCardPresent()) {
		return SdCardState::CardRemoved;
	}
	return _state;
}

SdCardType SdSdioBackend::cardType() const {
	return _cardType;
}

std::uint32_t SdSdioBackend::activeClockHz() const {
	return _activeClockHz;
}

SdTimingProfile SdSdioBackend::timingProfile() const {
	return {
	    .initializationClockHz = _pins.initializationClockHz,
	    .safeClockHz = _pins.initializationClockHz,
	    .defaultClockHz = _pins.transferClockHz,
	    .performanceClockHz = _pins.transferClockHz,
	    .maxClockHz = _pins.transferClockHz,
	};
}

bool SdSdioBackend::hasCardDetect() const {
	return _cardDetect.isValid();
}

bool SdSdioBackend::isCardInserted() const {
	return physicalCardPresent();
}

bool SdSdioBackend::setTransferBaudRate(std::uint32_t) {
	return false;
}

void SdSdioBackend::setDiagnosticsEnabled(bool enabled) {
	_diagnosticsEnabled = enabled;
}

bool SdSdioBackend::diagnosticsEnabled() const {
	return _diagnosticsEnabled;
}

void SdSdioBackend::clearDiagnostics() {
	_diagnosticSequence = 0;
	_diagnosticWriteIndex = 0;
	_diagnosticStoredCount = 0;
	_diagnosticDroppedCount = 0;
	_diagnosticEntries = {};
}

SdCardDiagnostics SdSdioBackend::diagnostics() {
	return diagnosticsUnlocked();
}

bool SdSdioBackend::configureCardDetect() {
	if (!_cardDetect.isValid()) {
		return true;
	}
	auto const& gpio = _platformServices.gpio;
	return gpio.configureInput != nullptr && gpio.configureInput(_cardDetect.detect, lexxos::platform::GpioPull::Up);
}

bool SdSdioBackend::physicalCardPresent() const {
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

bool SdSdioBackend::configurePeripheral() {
	return false;
}

bool SdSdioBackend::setClock(std::uint32_t) {
	return false;
}

SdSdioBackend::CommandResult SdSdioBackend::sendCommand(std::uint8_t, std::uint32_t, ResponseType) {
	return {};
}

SdSdioBackend::CommandResult SdSdioBackend::sendApplicationCommand(std::uint8_t, std::uint32_t, ResponseType) {
	return {};
}

bool SdSdioBackend::selectCard() {
	return false;
}

bool SdSdioBackend::configureBusWidth() {
	return false;
}

bool SdSdioBackend::readCsd() {
	return false;
}

bool SdSdioBackend::readSingleBlock(std::uint64_t, std::uint8_t*) {
	return false;
}

bool SdSdioBackend::writeSingleBlock(std::uint64_t, std::uint8_t const*) {
	return false;
}

void SdSdioBackend::resetState(SdCardState state) {
	_state = state;
	_cardType = SdCardType::Unknown;
	_blockCount = 0;
	_activeClockHz = 0;
	_rca = 0;
}

void SdSdioBackend::setFailure(SdCardState state) {
	_state = state;
	_cardType = SdCardType::Unknown;
	_blockCount = 0;
	_activeClockHz = 0;
	_rca = 0;
}

void SdSdioBackend::recordDiagnostic(
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
	entry.activeBaudRate = _activeClockHz;
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

SdCardDiagnostics SdSdioBackend::diagnosticsUnlocked() const {
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

std::uint32_t SdSdioBackend::uptimeMs() const {
	auto const& clock = _platformServices.clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0;
}

void SdSdioBackend::sleepMs(std::uint32_t milliseconds) const {
	auto const& clock = _platformServices.clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

std::uint32_t SdSdioBackend::commandAddress(std::uint64_t block) const {
	return _cardType == SdCardType::SdV2BlockAddressed ? static_cast<std::uint32_t>(block) : static_cast<std::uint32_t>(block * blockSize);
}

} // namespace lexxos::devices
