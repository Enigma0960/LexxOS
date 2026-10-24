#include <lexxos/devices/storage/sd_sdio_backend.h>

#include <cstdint>

#include <lexxos/devices/stm32/detail/stm32_sdio_support.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {
namespace {

	constexpr std::uint8_t cmd0GoIdle = 0;
	constexpr std::uint8_t cmd2AllSendCid = 2;
	constexpr std::uint8_t cmd3SendRelativeAddress = 3;
	constexpr std::uint8_t cmd7SelectCard = 7;
	constexpr std::uint8_t cmd8SendIfCond = 8;
	constexpr std::uint8_t cmd9SendCsd = 9;
	constexpr std::uint8_t cmd16SetBlockLength = 16;
	constexpr std::uint8_t cmd17ReadSingleBlock = 17;
	constexpr std::uint8_t cmd24WriteSingleBlock = 24;
	constexpr std::uint8_t cmd55ApplicationCommand = 55;
	constexpr std::uint8_t acmd6SetBusWidth = 6;
	constexpr std::uint8_t acmd41SendOpCond = 41;

	constexpr std::uint32_t acmd41Hcs = 1U << 30U;
	constexpr std::uint32_t acmd41VoltageWindow = 0x00FF8000U;
	constexpr std::uint32_t acmd41PowerUpReady = 1U << 31U;
	constexpr std::uint32_t acmd41Ccs = 1U << 30U;

} // namespace

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

	// Этап 1: проверяем card-detect и подготавливаем выводы/периферию SDIO.
	if (!configureCardDetect()) {
		recordDiagnostic(SdCardDiagnosticCode::InitializeFailed);
		setFailure(SdCardState::NoBus);
		return false;
	}
	if (!physicalCardPresent()) {
		recordDiagnostic(SdCardDiagnosticCode::CardRemoved);
		setFailure(SdCardState::NoCard);
		return false;
	}
	if (!_pins.isValid() || !configurePeripheral()) {
		recordDiagnostic(SdCardDiagnosticCode::InitializeFailed);
		setFailure(SdCardState::NoBus);
		return false;
	}

	if (!setClock(_pins.initializationClockHz == 0 ? 400000U : _pins.initializationClockHz)) {
		recordDiagnostic(SdCardDiagnosticCode::SetBaudFailed);
		setFailure(SdCardState::NoBus);
		return false;
	}
	sleepMs(2);

	// Этап 2: переводим карту в idle и согласовываем SD v2 + рабочее напряжение.
	(void) sendCommand(cmd0GoIdle, 0, ResponseType::None);
	sleepMs(2);

	auto cmd8 = sendCommand(cmd8SendIfCond, 0x000001AAU, ResponseType::Short);
	if (!cmd8.ok || (cmd8.response[0] & 0x00000FFFU) != 0x000001AAU) {
		recordDiagnostic(SdCardDiagnosticCode::CommandResponseReadFailed, cmd8SendIfCond);
		setFailure(SdCardState::Unsupported);
		return false;
	}

	std::uint32_t ocr = 0;
	bool enteredReadyState = false;
	for (std::uint16_t attempt = 0; attempt < 1000; ++attempt) {
		auto acmd41 = sendApplicationCommand(acmd41SendOpCond, acmd41Hcs | acmd41VoltageWindow, ResponseType::ShortNoCrc);
		if (acmd41.ok) {
			ocr = acmd41.response[0];
			if ((ocr & acmd41PowerUpReady) != 0U) {
				enteredReadyState = true;
				break;
			}
		}
		sleepMs(2);
	}
	if (!enteredReadyState) {
		recordDiagnostic(SdCardDiagnosticCode::CommandResponseTimeout, acmd41SendOpCond);
		setFailure(SdCardState::Timeout);
		return false;
	}

	_cardType = (ocr & acmd41Ccs) != 0U ? SdCardType::SdV2BlockAddressed : SdCardType::SdV2ByteAddressed;

	// Этап 3: получаем CID/RCA/CSD, выбираем карту и фиксируем однобитную шину.
	auto cid = sendCommand(cmd2AllSendCid, 0, ResponseType::Long);
	if (!cid.ok) {
		recordDiagnostic(SdCardDiagnosticCode::CommandResponseReadFailed, cmd2AllSendCid);
		setFailure(SdCardState::IoError);
		return false;
	}

	auto cmd3 = sendCommand(cmd3SendRelativeAddress, 0, ResponseType::Short);
	if (!cmd3.ok) {
		recordDiagnostic(SdCardDiagnosticCode::CommandResponseReadFailed, cmd3SendRelativeAddress);
		setFailure(SdCardState::IoError);
		return false;
	}
	_rca = static_cast<std::uint16_t>((cmd3.response[0] >> 16U) & 0xFFFFU);
	if (_rca == 0) {
		recordDiagnostic(SdCardDiagnosticCode::CommandResponseReadFailed, cmd3SendRelativeAddress);
		setFailure(SdCardState::IoError);
		return false;
	}

	if (!readCsd()) {
		recordDiagnostic(SdCardDiagnosticCode::RegisterReadFailed, cmd9SendCsd);
		setFailure(SdCardState::IoError);
		return false;
	}
	if (!selectCard() || !configureBusWidth()) {
		recordDiagnostic(SdCardDiagnosticCode::SetModeFailed);
		setFailure(SdCardState::IoError);
		return false;
	}

	if (_cardType != SdCardType::SdV2BlockAddressed) {
		auto cmd16 = sendCommand(cmd16SetBlockLength, blockSize, ResponseType::Short);
		if (!cmd16.ok) {
			recordDiagnostic(SdCardDiagnosticCode::CommandResponseReadFailed, cmd16SetBlockLength);
			setFailure(SdCardState::Unsupported);
			return false;
		}
	}

	// Этап 4: только после полной идентификации переключаемся на transfer clock.
	if (!setClock(_pins.transferClockHz == 0 ? 24000000U : _pins.transferClockHz)) {
		recordDiagnostic(SdCardDiagnosticCode::SetBaudFailed);
		setFailure(SdCardState::IoError);
		return false;
	}

	_state = SdCardState::Ready;
	return true;
}

bool SdSdioBackend::isReady() const {
	return _state == SdCardState::Ready && physicalCardPresent();
}

std::uint32_t SdSdioBackend::blockSizeBytes() const {
	return blockSize;
}

std::uint64_t SdSdioBackend::blockCount() const {
	return isReady() ? _blockCount : 0;
}

bool SdSdioBackend::readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t blockCount) {
	if (!isReady() || buffer == nullptr) {
		if (!physicalCardPresent()) {
			setFailure(SdCardState::CardRemoved);
		}
		return false;
	}

	auto* output = static_cast<std::uint8_t*>(buffer);
	for (std::size_t index = 0; index < blockCount; ++index) {
		if (!readSingleBlock(firstBlock + index, output + index * blockSize)) {
			setFailure(SdCardState::IoError);
			return false;
		}
	}
	return true;
}

bool SdSdioBackend::writeBlocks(std::uint64_t firstBlock, void const* buffer, std::size_t blockCount) {
	if (!isReady() || buffer == nullptr) {
		if (!physicalCardPresent()) {
			setFailure(SdCardState::CardRemoved);
		}
		return false;
	}

	auto const* input = static_cast<std::uint8_t const*>(buffer);
	for (std::size_t index = 0; index < blockCount; ++index) {
		if (!writeSingleBlock(firstBlock + index, input + index * blockSize)) {
			setFailure(SdCardState::IoError);
			return false;
		}
	}
	return true;
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

bool SdSdioBackend::setTransferBaudRate(std::uint32_t baudRate) {
	if (baudRate == 0) {
		recordDiagnostic(SdCardDiagnosticCode::SetBaudFailed, 0xFFU, baudRate);
		return false;
	}
	_pins.transferClockHz = baudRate;
	if (_state != SdCardState::Ready) {
		return true;
	}
	return setClock(baudRate);
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
	return stm32::detail::configureSdioPeripheral(_pins);
}

bool SdSdioBackend::setClock(std::uint32_t clockHz) {
	return stm32::detail::setSdioClock(clockHz, _activeClockHz);
}

SdSdioBackend::CommandResult SdSdioBackend::sendCommand(std::uint8_t command, std::uint32_t argument, ResponseType responseType) {
	stm32::detail::SdioResponseType supportResponseType = stm32::detail::SdioResponseType::None;
	switch (responseType) {
		case ResponseType::None:
			break;
		case ResponseType::Short:
			supportResponseType = stm32::detail::SdioResponseType::Short;
			break;
		case ResponseType::ShortNoCrc:
			supportResponseType = stm32::detail::SdioResponseType::ShortNoCrc;
			break;
		case ResponseType::Long:
			supportResponseType = stm32::detail::SdioResponseType::Long;
			break;
	}

	auto const commandResult = stm32::detail::sendSdioCommand(command, argument, supportResponseType);
	CommandResult result{};
	result.ok = commandResult.ok;
	for (std::size_t index = 0; index < 4; ++index) {
		result.response[index] = commandResult.response[index];
	}
	return result;
}

SdSdioBackend::CommandResult SdSdioBackend::sendApplicationCommand(
    std::uint8_t command,
    std::uint32_t argument,
    ResponseType responseType) {
	auto appPrefix = sendCommand(cmd55ApplicationCommand, static_cast<std::uint32_t>(_rca) << 16U, ResponseType::Short);
	if (!appPrefix.ok) {
		return {};
	}
	return sendCommand(command, argument, responseType);
}

bool SdSdioBackend::selectCard() {
	auto result = sendCommand(cmd7SelectCard, static_cast<std::uint32_t>(_rca) << 16U, ResponseType::Short);
	return result.ok;
}

bool SdSdioBackend::configureBusWidth() {
	stm32::detail::configureSdioOneBitBus();
	auto result = sendApplicationCommand(acmd6SetBusWidth, 0U, ResponseType::Short);
	return result.ok;
}

bool SdSdioBackend::readCsd() {
	auto csd = sendCommand(cmd9SendCsd, static_cast<std::uint32_t>(_rca) << 16U, ResponseType::Long);
	if (!csd.ok) {
		return false;
	}

	std::uint32_t const structure = stm32::detail::readCsdBits(csd.response, 127, 126);
	if (structure == 1U) {
		std::uint32_t const cSize = stm32::detail::readCsdBits(csd.response, 69, 48);
		_blockCount = (static_cast<std::uint64_t>(cSize) + 1ULL) * 1024ULL;
		return _blockCount > 0;
	}
	if (structure == 0U) {
		std::uint32_t const readBlockLength = stm32::detail::readCsdBits(csd.response, 83, 80);
		std::uint32_t const cSize = stm32::detail::readCsdBits(csd.response, 73, 62);
		std::uint32_t const cSizeMult = stm32::detail::readCsdBits(csd.response, 49, 47);
		std::uint64_t const blockNumber = (static_cast<std::uint64_t>(cSize) + 1ULL) << (cSizeMult + 2U);
		std::uint64_t const blockLength = 1ULL << readBlockLength;
		_blockCount = (blockNumber * blockLength) / blockSize;
		return _blockCount > 0;
	}
	return false;
}

bool SdSdioBackend::readSingleBlock(std::uint64_t block, std::uint8_t* buffer) {
	if (buffer == nullptr) {
		return false;
	}

	stm32::detail::prepareSdioBlockRead();

	auto command = sendCommand(cmd17ReadSingleBlock, commandAddress(block), ResponseType::Short);
	if (!command.ok) {
		stm32::detail::cancelSdioDataTransfer();
		return false;
	}
	return stm32::detail::finishSdioBlockRead(buffer, blockSize);
}

bool SdSdioBackend::writeSingleBlock(std::uint64_t block, std::uint8_t const* buffer) {
	if (buffer == nullptr) {
		return false;
	}

	stm32::detail::prepareSdioBlockWrite();

	auto command = sendCommand(cmd24WriteSingleBlock, commandAddress(block), ResponseType::Short);
	if (!command.ok) {
		stm32::detail::cancelSdioDataTransfer();
		return false;
	}
	return stm32::detail::finishSdioBlockWrite(buffer, blockSize);
}

void SdSdioBackend::resetState(SdCardState state) {
	_state = state;
	_cardType = SdCardType::Unknown;
	_rca = 0;
	_blockCount = 0;
	_activeClockHz = 0;
}

void SdSdioBackend::setFailure(SdCardState state) {
	_state = state;
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
	std::uint64_t const address = _cardType == SdCardType::SdV2BlockAddressed ? block : block * blockSize;
	return static_cast<std::uint32_t>(address & 0xFFFFFFFFU);
}

} // namespace lexxos::devices
