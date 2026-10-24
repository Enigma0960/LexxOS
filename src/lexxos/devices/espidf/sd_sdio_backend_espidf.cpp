#include <lexxos/devices/storage/sd_sdio_backend.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <driver/sdmmc_host.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>
#include <sdkconfig.h>
#include <sdmmc_cmd.h>
#include <soc/soc_caps.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {
namespace {

	constexpr std::size_t dmaBounceBlocks = 8;
	constexpr std::size_t dmaBounceBytes = SdSdioBackend::blockSize * dmaBounceBlocks;
	constexpr std::uint8_t cmd17ReadSingleBlock = 17;
	constexpr std::uint8_t cmd24WriteSingleBlock = 24;
	constexpr std::uint32_t ocrCardCapacityStatus = 1U << 30U;

	gpio_num_t toGpio(lexxos::platform::GpioPin pin) {
		return pin.isValid() ? static_cast<gpio_num_t>(pin.number) : GPIO_NUM_NC;
	}

	int toSlotIndex(int index) {
		return index == 0 ? SDMMC_HOST_SLOT_0 : SDMMC_HOST_SLOT_1;
	}

	SdCardState stateForError(esp_err_t result) {
		switch (result) {
			case ESP_OK:
				return SdCardState::Ready;
			case ESP_ERR_TIMEOUT:
				return SdCardState::Timeout;
			case ESP_ERR_NOT_SUPPORTED:
				return SdCardState::Unsupported;
			case ESP_ERR_INVALID_STATE:
			case ESP_ERR_INVALID_ARG:
				return SdCardState::NoBus;
			default:
				return SdCardState::IoError;
		}
	}

	SdCardType typeForCard(sdmmc_card_t const& card) {
		if (card.is_mmc != 0U) {
			return SdCardType::Unknown;
		}
		return (card.ocr & ocrCardCapacityStatus) != 0U ? SdCardType::SdV2BlockAddressed : SdCardType::SdV2ByteAddressed;
	}

	struct EspIdfSdioRuntime {
		sdmmc_host_t host{};
		sdmmc_card_t card{};
		sd_pwr_ctrl_handle_t powerControl{nullptr};
		void* dmaBuffer{nullptr};
		bool hostInitialized{false};
		bool hostSlotInitialized{false};
		bool sharedHostedHost{false};
	};

	EspIdfSdioRuntime g_runtime{};

#if defined(LEXXOS_ESPIDF_ENABLE_HOSTED) && defined(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE)
	esp_err_t sharedHostedHostInit() {
		return ESP_OK;
	}

	bool shouldShareHostedSdioHost(lexxos::platform::SdioPins const& pins) {
		return pins.index == 0;
	}

	bool hostedSdioHostReady() {
		sdmmc_host_state_t state{};
		return sdmmc_host_get_state(&state) == ESP_OK && state.host_initialized;
	}
#else
	bool shouldShareHostedSdioHost(lexxos::platform::SdioPins const&) {
		return false;
	}

	bool hostedSdioHostReady() {
		return false;
	}
#endif

	void releaseRuntime() {
		if (g_runtime.hostInitialized) {
			if (g_runtime.hostSlotInitialized && g_runtime.host.deinit_p != nullptr) {
				(void) g_runtime.host.deinit_p(g_runtime.host.slot);
			} else if (!g_runtime.sharedHostedHost) {
				(void) sdmmc_host_deinit();
			}
		}
		g_runtime.hostInitialized = false;
		g_runtime.hostSlotInitialized = false;

		if (g_runtime.powerControl != nullptr) {
			(void) sd_pwr_ctrl_del_on_chip_ldo(g_runtime.powerControl);
			g_runtime.powerControl = nullptr;
		}

		if (g_runtime.dmaBuffer != nullptr) {
			heap_caps_free(g_runtime.dmaBuffer);
			g_runtime.dmaBuffer = nullptr;
		}

		g_runtime.host = {};
		g_runtime.card = {};
		g_runtime.sharedHostedHost = false;
	}

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
	if (_state == SdCardState::Ready && physicalCardPresent() && sdmmc_get_status(&g_runtime.card) == ESP_OK) {
		return true;
	}

	releaseRuntime();
	resetState();

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
	if (!_pins.isValid()) {
		recordDiagnostic(SdCardDiagnosticCode::InitializeFailed);
		setFailure(SdCardState::NoBus);
		releaseRuntime();
		return false;
	}
	if (!configurePeripheral()) {
		setFailure(SdCardState::NoBus);
		releaseRuntime();
		return false;
	}

	auto result = sdmmc_card_init(&g_runtime.host, &g_runtime.card);
	if (result != ESP_OK) {
		recordDiagnostic(SdCardDiagnosticCode::SdioCardInitFailed, 0xFFU, static_cast<std::uint32_t>(result));
		setFailure(stateForError(result));
		releaseRuntime();
		return false;
	}

	_state = SdCardState::Ready;
	_cardType = typeForCard(g_runtime.card);
	_blockCount = static_cast<std::uint64_t>(std::max(g_runtime.card.csd.capacity, 0));
	_activeClockHz = static_cast<std::uint32_t>(std::max(g_runtime.card.real_freq_khz, 0) * 1000);
	if (_blockCount == 0) {
		recordDiagnostic(SdCardDiagnosticCode::InitializeFailed);
		return false;
	}
	return true;
}

bool SdSdioBackend::isReady() const {
	return _state == SdCardState::Ready && physicalCardPresent();
}

std::uint32_t SdSdioBackend::blockSizeBytes() const {
	if (_state == SdCardState::Ready && g_runtime.card.csd.sector_size > 0) {
		return static_cast<std::uint32_t>(g_runtime.card.csd.sector_size);
	}
	return blockSize;
}

std::uint64_t SdSdioBackend::blockCount() const {
	return isReady() ? _blockCount : 0;
}

bool SdSdioBackend::readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t blockCount) {
	if (buffer == nullptr) {
		recordDiagnostic(SdCardDiagnosticCode::NullBuffer, cmd17ReadSingleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		return false;
	}
	if (!isReady()) {
		if (!physicalCardPresent()) {
			recordDiagnostic(SdCardDiagnosticCode::CardRemoved, cmd17ReadSingleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
			setFailure(SdCardState::CardRemoved);
		} else {
			recordDiagnostic(SdCardDiagnosticCode::NotReady, cmd17ReadSingleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		}
		return false;
	}
	if (firstBlock > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
		recordDiagnostic(SdCardDiagnosticCode::TransferFailure, cmd17ReadSingleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		setFailure(SdCardState::IoError);
		return false;
	}

	auto const result = sdmmc_read_sectors(&g_runtime.card, buffer, static_cast<std::size_t>(firstBlock), blockCount);
	if (result != ESP_OK) {
		recordDiagnostic(SdCardDiagnosticCode::TransferFailure, cmd17ReadSingleBlock, static_cast<std::uint32_t>(result), firstBlock, static_cast<std::uint32_t>(blockCount));
		setFailure(stateForError(result));
		return false;
	}
	return true;
}

bool SdSdioBackend::writeBlocks(std::uint64_t firstBlock, void const* buffer, std::size_t blockCount) {
	if (buffer == nullptr) {
		recordDiagnostic(SdCardDiagnosticCode::NullBuffer, cmd24WriteSingleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		return false;
	}
	if (!isReady()) {
		if (!physicalCardPresent()) {
			recordDiagnostic(SdCardDiagnosticCode::CardRemoved, cmd24WriteSingleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
			setFailure(SdCardState::CardRemoved);
		} else {
			recordDiagnostic(SdCardDiagnosticCode::NotReady, cmd24WriteSingleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		}
		return false;
	}
	if (firstBlock > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
		recordDiagnostic(SdCardDiagnosticCode::TransferFailure, cmd24WriteSingleBlock, 0xFFFFFFFFU, firstBlock, static_cast<std::uint32_t>(blockCount));
		setFailure(SdCardState::IoError);
		return false;
	}

	auto const result = sdmmc_write_sectors(&g_runtime.card, buffer, static_cast<std::size_t>(firstBlock), blockCount);
	if (result != ESP_OK) {
		recordDiagnostic(SdCardDiagnosticCode::TransferFailure, cmd24WriteSingleBlock, static_cast<std::uint32_t>(result), firstBlock, static_cast<std::uint32_t>(blockCount));
		setFailure(stateForError(result));
		return false;
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
	g_runtime.host = SDMMC_HOST_DEFAULT();
	g_runtime.host.slot = toSlotIndex(_pins.index);
	g_runtime.sharedHostedHost = shouldShareHostedSdioHost(_pins) && hostedSdioHostReady();
#if defined(LEXXOS_ESPIDF_ENABLE_HOSTED) && defined(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE)
	if (g_runtime.sharedHostedHost) {
		g_runtime.host.init = &sharedHostedHostInit;
	}
#endif
	g_runtime.host.flags &= ~SDMMC_HOST_FLAG_8BIT;
	if (!_pins.supportsWideBus()) {
		g_runtime.host.flags &= ~SDMMC_HOST_FLAG_4BIT;
	}
	if (_pins.transferClockHz != 0) {
		g_runtime.host.max_freq_khz = static_cast<int>(_pins.transferClockHz / 1000U);
	}
	g_runtime.host.unaligned_multi_block_rw_max_chunk_size = dmaBounceBlocks;
	g_runtime.host.dma_aligned_buffer = heap_caps_aligned_alloc(64, dmaBounceBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	if (g_runtime.host.dma_aligned_buffer == nullptr) {
		recordDiagnostic(SdCardDiagnosticCode::DmaStartFailed);
		return false;
	}
	g_runtime.dmaBuffer = g_runtime.host.dma_aligned_buffer;

#if SOC_SDMMC_IO_POWER_EXTERNAL
	if (_pins.powerLdoChannel >= 0) {
		sd_pwr_ctrl_ldo_config_t ldoConfig{
		    .ldo_chan_id = _pins.powerLdoChannel,
		};
		auto const result = sd_pwr_ctrl_new_on_chip_ldo(&ldoConfig, &g_runtime.powerControl);
		if (result != ESP_OK) {
			recordDiagnostic(SdCardDiagnosticCode::SdioPowerControlFailed, 0xFFU, static_cast<std::uint32_t>(result));
			return false;
		}
		g_runtime.host.pwr_ctrl_handle = g_runtime.powerControl;
	}
#endif

	sdmmc_slot_config_t slotConfig = SDMMC_SLOT_CONFIG_DEFAULT();
	slotConfig.width = _pins.supportsWideBus() ? 4 : 1;
	slotConfig.clk = toGpio(_pins.clock);
	slotConfig.cmd = toGpio(_pins.command);
	slotConfig.d0 = toGpio(_pins.data0);
	slotConfig.d1 = toGpio(_pins.data1);
	slotConfig.d2 = toGpio(_pins.data2);
	slotConfig.d3 = toGpio(_pins.data3);
	slotConfig.cd = _cardDetect.isValid() ? toGpio(_cardDetect.detect) : SDMMC_SLOT_NO_CD;
	slotConfig.wp = SDMMC_SLOT_NO_WP;
	slotConfig.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

	auto result = g_runtime.host.init();
	if (result != ESP_OK) {
		recordDiagnostic(SdCardDiagnosticCode::SdioHostInitFailed, 0xFFU, static_cast<std::uint32_t>(result));
		return false;
	}
	g_runtime.hostInitialized = true;
	result = sdmmc_host_init_slot(g_runtime.host.slot, &slotConfig);
	if (result != ESP_OK) {
		recordDiagnostic(SdCardDiagnosticCode::SdioSlotInitFailed, 0xFFU, static_cast<std::uint32_t>(result));
		return false;
	}
	g_runtime.hostSlotInitialized = true;
	return true;
}

bool SdSdioBackend::setClock(std::uint32_t clockHz) {
	if (!g_runtime.hostSlotInitialized || g_runtime.host.set_card_clk == nullptr) {
		return false;
	}
	auto result = g_runtime.host.set_card_clk(g_runtime.host.slot, clockHz / 1000U);
	if (result != ESP_OK) {
		recordDiagnostic(SdCardDiagnosticCode::SetBaudFailed, 0xFFU, static_cast<std::uint32_t>(result));
		return false;
	}
	int realFreqKhz = 0;
	if (g_runtime.host.get_real_freq != nullptr && g_runtime.host.get_real_freq(g_runtime.host.slot, &realFreqKhz) == ESP_OK) {
		_activeClockHz = static_cast<std::uint32_t>(std::max(realFreqKhz, 0) * 1000);
	} else {
		_activeClockHz = clockHz;
	}
	return true;
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
