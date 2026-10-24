#include <sim_sd_card.h>

#include <lexxos/devices/sd_card.h>

#include <sim_runtime.h>

#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

namespace {

lexxos::platform::PlatformServices const* g_platformServices{nullptr};

void bindPlatform(lexxos::platform::PlatformServices const& platform) {
	g_platformServices = &platform;
}

[[nodiscard]] lexxos::platform::PlatformServices const& activePlatform() {
	return g_platformServices != nullptr ? *g_platformServices : lexxos::platform::services();
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

} // namespace

namespace lexxos::emulator::detail {

using lexxos::devices::SdCardInterface;
using lexxos::devices::SdCardState;
using lexxos::devices::SdCardType;

bool SimSdCardBlockDevice::initialize() {
	if (!inserted()) {
		_state = SdCardState::NoCard;
		_blockCount = 0;
		return false;
	}

	auto* runtime = activeRuntime();
	if (runtime == nullptr) {
		_state = SdCardState::NoBus;
		_blockCount = 0;
		return false;
	}

	_blockCount = runtime->blockCount(deviceId);
	if (_blockCount == 0) {
		_state = SdCardState::IoError;
		return false;
	}

	_state = SdCardState::Ready;
	return true;
}

bool SimSdCardBlockDevice::isReady() const {
	return _state == SdCardState::Ready && inserted();
}

bool SimSdCardBlockDevice::isReadOnly() const {
	auto* runtime = activeRuntime();
	return runtime != nullptr && runtime->blockIsReadOnly(deviceId);
}

std::uint32_t SimSdCardBlockDevice::blockSizeBytes() const {
	return SimRuntime::sdBlockSize;
}

std::uint64_t SimSdCardBlockDevice::blockCount() const {
	return isReady() ? _blockCount : 0;
}

bool SimSdCardBlockDevice::readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t count) {
	if (!isReady() || buffer == nullptr) {
		_state = inserted() ? SdCardState::IoError : SdCardState::CardRemoved;
		return false;
	}

	auto* runtime = activeRuntime();
	if (runtime == nullptr || !runtime->blockRead(deviceId, firstBlock, buffer, count)) {
		_state = inserted() ? SdCardState::IoError : SdCardState::CardRemoved;
		return false;
	}

	return true;
}

bool SimSdCardBlockDevice::writeBlocks(std::uint64_t firstBlock, void const* buffer, std::size_t count) {
	if (!isReady() || buffer == nullptr) {
		_state = inserted() ? SdCardState::IoError : SdCardState::CardRemoved;
		return false;
	}

	auto* runtime = activeRuntime();
	auto const result = runtime == nullptr ? LEXXOS_SIM_ERROR : runtime->blockWrite(deviceId, firstBlock, buffer, count);
	if (result == LEXXOS_SIM_WRITE_PROTECTED) {
		// A rejected write must not make a read-only image unavailable for reads.
		return false;
	}
	if (result != LEXXOS_SIM_OK) {
		_state = inserted() ? SdCardState::IoError : SdCardState::CardRemoved;
		return false;
	}

	return true;
}

SdCardInterface SimSdCardBlockDevice::interfaceType() const {
	return SdCardInterface::Simulated;
}

SdCardState SimSdCardBlockDevice::state() const {
	if (_state == SdCardState::Ready && !inserted()) {
		return SdCardState::CardRemoved;
	}
	return _state;
}

SdCardType SimSdCardBlockDevice::cardType() const {
	return _state == SdCardState::Ready ? SdCardType::SdV2BlockAddressed : SdCardType::Unknown;
}

std::uint32_t SimSdCardBlockDevice::activeClockHz() const {
	return 0;
}

bool SimSdCardBlockDevice::hasCardDetect() const {
	return boardFor(activePlatform()).sdCardDetect.isValid();
}

bool SimSdCardBlockDevice::isCardInserted() const {
	return inserted();
}

bool SimSdCardBlockDevice::inserted() const {
	auto* runtime = activeRuntime();
	return runtime != nullptr && runtime->blockIsInserted(deviceId);
}

} // namespace lexxos::emulator::detail

namespace {

lexxos::emulator::detail::SimSdCardBlockDevice& device() {
	static lexxos::emulator::detail::SimSdCardBlockDevice storage;
	return storage;
}

} // namespace

namespace lexxos::devices::sd_card {

bool initialize() {
	return device().initialize();
}

bool initialize(lexxos::platform::PlatformServices const& platform) {
	bindPlatform(platform);
	return initialize();
}

bool probe(std::uint8_t attempts) {
	return probe(lexxos::platform::services(), attempts);
}

bool probe(lexxos::platform::PlatformServices const& platform, std::uint8_t attempts) {
	bindPlatform(platform);
	if (attempts == 0) {
		attempts = 1;
	}

	for (std::uint8_t attempt = 0; attempt < attempts; ++attempt) {
		if (initialize()) {
			return true;
		}
		if (attempt + 1 < attempts) {
			sleepMs(platform, static_cast<std::uint32_t>(5U + attempt * 5U));
		}
	}
	return false;
}

bool isReady() {
	return device().isReady();
}

SdCardInterface interfaceType() {
	return device().interfaceType();
}

lexxos::platform::SdInterface boardInterface() {
	return boardInterface(lexxos::platform::services());
}

lexxos::platform::SdInterface boardInterface(lexxos::platform::PlatformServices const& platform) {
	bindPlatform(platform);
	return lexxos::platform::effectiveSdInterface(boardFor(platform));
}

bool backendMatchesBoardInterface() {
	return interfaceType() == SdCardInterface::Simulated;
}

bool backendMatchesBoardInterface(lexxos::platform::PlatformServices const& platform) {
	bindPlatform(platform);
	return backendMatchesBoardInterface();
}

SdCardState state() {
	return device().state();
}

SdCardType cardType() {
	return device().cardType();
}

std::uint32_t blockSizeBytes() {
	return device().blockSizeBytes();
}

std::uint64_t blockCount() {
	return device().blockCount();
}

std::uint32_t activeBaudRate() {
	return device().activeClockHz();
}

SdTimingProfile timingProfile() {
	return device().timingProfile();
}

bool setTransferBaudRate(std::uint32_t baudRate) {
	return device().setTransferBaudRate(baudRate);
}

bool setSpiClockMode(SpiClockMode mode) {
	return device().setSpiClockMode(mode);
}

SpiClockMode spiClockMode() {
	return device().spiClockMode();
}

void setReadCrcValidation(bool enabled) {
	device().setReadCrcValidation(enabled);
}

bool readCrcValidation() {
	return device().readCrcValidation();
}

std::uint32_t readRecoveryReinitializations() {
	return device().readRecoveryReinitializations();
}

void setMultiBlockRead(bool enabled) {
	device().setMultiBlockRead(enabled);
}

bool multiBlockReadEnabled() {
	return device().multiBlockReadEnabled();
}

void setMultiBlockWrite(bool enabled) {
	device().setMultiBlockWrite(enabled);
}

bool multiBlockWriteEnabled() {
	return device().multiBlockWriteEnabled();
}

bool dmaSupported() {
	return device().dmaSupported();
}

void setDmaTransfersEnabled(bool enabled) {
	device().setDmaTransfersEnabled(enabled);
}

bool dmaTransfersEnabled() {
	return device().dmaTransfersEnabled();
}

void setHighSpeedWritesEnabled(bool enabled) {
	device().setHighSpeedWritesEnabled(enabled);
}

bool highSpeedWritesEnabled() {
	return device().highSpeedWritesEnabled();
}

void setDiagnosticsEnabled(bool enabled) {
	device().setDiagnosticsEnabled(enabled);
}

bool diagnosticsEnabled() {
	return device().diagnosticsEnabled();
}

void clearDiagnostics() {
	device().clearDiagnostics();
}

SdCardDiagnostics diagnostics() {
	return device().diagnostics();
}

bool hasCardDetect() {
	return device().hasCardDetect();
}

bool isCardInserted() {
	return device().isCardInserted();
}

BlockDevice* blockDevice() {
	return &device();
}

BlockDevice* blockDevice(lexxos::platform::PlatformServices const& platform) {
	bindPlatform(platform);
	return blockDevice();
}

} // namespace lexxos::devices::sd_card
