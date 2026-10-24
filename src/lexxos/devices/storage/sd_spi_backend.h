#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/devices/storage/sd_card_backend.h>
#include <lexxos/devices/storage/sd_spi_block_ops.h>
#include <lexxos/devices/storage/sd_spi_protocol.h>
#include <lexxos/devices/storage/sd_spi_transport.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

class SdSpiBackend final : public SdCardBackend {
public:
	static constexpr std::uint32_t blockSize = 512;
	static constexpr std::uint32_t initializationBaudRate = 400000;
	static constexpr std::uint32_t safeTransferBaudRate = 1000000;
	static constexpr std::uint32_t defaultTransferBaudRate = 50000000;
	static constexpr std::uint32_t performanceTransferBaudRate = 60000000;
	static constexpr std::uint32_t maxTransferBaudRate = 62000000;
	static constexpr std::uint32_t safeWriteBaudRate = defaultTransferBaudRate;
	static constexpr std::uint32_t dmaTransferTimeoutMs = 1000;

	SdSpiBackend(SpiBus& bus, lexxos::platform::SpiPins pins, lexxos::platform::GpioCardDetect cardDetect = {});
	SdSpiBackend(
	    SpiBus& bus,
	    lexxos::platform::SpiPins pins,
	    lexxos::platform::GpioCardDetect cardDetect,
	    lexxos::platform::PlatformServices const& platform);

	bool initialize() override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] std::uint32_t blockSizeBytes() const override;
	[[nodiscard]] std::uint64_t blockCount() const override;
	bool readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t blockCount) override;
	bool writeBlocks(std::uint64_t firstBlock, void const* buffer, std::size_t blockCount) override;

	[[nodiscard]] SdCardInterface interfaceType() const override;
	[[nodiscard]] SdCardState state() const override;
	[[nodiscard]] SdCardType cardType() const override;
	[[nodiscard]] std::uint32_t activeBaudRate() const;
	[[nodiscard]] std::uint32_t activeClockHz() const override;
	[[nodiscard]] SdTimingProfile timingProfile() const override;
	bool setTransferBaudRate(std::uint32_t baudRate) override;
	bool setSpiClockMode(SpiClockMode mode) override;
	[[nodiscard]] SpiClockMode spiClockMode() const override;
	void setReadCrcValidation(bool enabled) override;
	[[nodiscard]] bool readCrcValidation() const override;
	void setMultiBlockRead(bool enabled) override;
	void setMultiBlockWrite(bool enabled) override;
	[[nodiscard]] bool multiBlockWriteEnabled() const override;
	[[nodiscard]] bool multiBlockReadEnabled() const override;
	[[nodiscard]] bool dmaSupported() const override;
	void setDmaTransfersEnabled(bool enabled) override;
	[[nodiscard]] bool dmaTransfersEnabled() const override;
	void setHighSpeedWritesEnabled(bool enabled) override;
	[[nodiscard]] bool highSpeedWritesEnabled() const override;
	void setDiagnosticsEnabled(bool enabled) override;
	[[nodiscard]] bool diagnosticsEnabled() const override;
	void clearDiagnostics() override;
	[[nodiscard]] SdCardDiagnostics diagnostics() override;
	[[nodiscard]] std::uint32_t readRecoveryReinitializations() const override;
	[[nodiscard]] bool hasCardDetect() const override;
	[[nodiscard]] bool isCardInserted() const override;

private:
	static constexpr std::uint8_t commandTimeoutResponse = 0xFF;

	bool clockIdle(std::size_t byteCount);
	bool readByte(std::uint8_t& value);
	bool writeByte(std::uint8_t value);
	bool waitReady(std::uint32_t attempts);
	bool waitDataToken(std::uint32_t attempts);
	std::uint8_t sendCommand(
	    std::uint8_t command,
	    std::uint32_t argument,
	    std::uint8_t crc,
	    std::uint8_t* response,
	    std::size_t responseSize,
	    bool keepSelected);
	std::uint8_t sendCommand(std::uint8_t command, std::uint32_t argument, std::uint8_t crc, bool keepSelected = false);
	std::uint8_t sendApplicationCommand(std::uint8_t command, std::uint32_t argument);
	bool readRegister(std::uint8_t command, std::array<std::uint8_t, 16>& registerData);
	bool readSingleBlock(std::uint64_t block, std::uint8_t* buffer);
	bool readSingleBlockWithRecovery(std::uint64_t block, std::uint8_t* buffer);
	bool readMultipleBlocks(std::uint64_t firstBlock, std::uint8_t* buffer, std::size_t blockCount);
	bool readMultipleBlocksWithRecovery(std::uint64_t firstBlock, std::uint8_t* buffer, std::size_t blockCount);
	bool recoverReadPath(std::uint64_t block, std::uint8_t* buffer);
	bool writeSingleBlockWithRecovery(std::uint64_t block, std::uint8_t const* buffer);
	bool writeSingleBlock(std::uint64_t block, std::uint8_t const* buffer);
	bool writeMultipleBlocksWithRecovery(std::uint64_t firstBlock, std::uint8_t const* buffer, std::size_t blockCount);
	bool writeMultipleBlocks(std::uint64_t firstBlock, std::uint8_t const* buffer, std::size_t blockCount);
	bool initializeCard();
	[[nodiscard]] bool writeTransferAllowed() const;
	bool readCsd();
	bool configureCardDetect();
	[[nodiscard]] bool physicalCardPresent() const;
	bool setActiveBaudRate(std::uint32_t baudRate);
	void markTransferFailure();
	void recordDiagnostic(
	    SdCardDiagnosticCode code,
	    std::uint32_t command = 0xFFU,
	    std::uint32_t r1 = 0xFFFFFFFFU,
	    std::uint64_t block = 0,
	    std::uint32_t blockCount = 0);
	[[nodiscard]] SdCardDiagnostics diagnosticsUnlocked() const;
	[[nodiscard]] SdCardState classifyMissingCardState() const;
	[[nodiscard]] SdCardState classifyCardAfterFailure();
	[[nodiscard]] std::uint32_t uptimeMs() const;
	void sleepMs(std::uint32_t milliseconds) const;
	[[nodiscard]] bool validateReadCrc(std::uint8_t const* data, std::size_t size, std::array<std::uint8_t, 2> const& crc) const;
	[[nodiscard]] SdSpiBlockTransferConfig blockTransferConfig() const;
	void recordBlockDiagnostic(SdSpiBlockResult const& result);
	void endTransaction();
	[[nodiscard]] std::uint32_t commandAddress(std::uint64_t block) const;
	void setFailure(SdCardState state);

	lexxos::platform::PlatformServices const& _platformServices;
	SdSpiTransport _transport;
	SdSpiProtocol _protocol;
	SdSpiBlockOps _blockOps;

	lexxos::platform::GpioCardDetect _cardDetect;
	SdCardState _state{SdCardState::Uninitialized};
	SdCardType _cardType{SdCardType::Unknown};
	std::uint64_t _blockCount{0};
	std::uint32_t _activeBaudRate{0};
	std::uint32_t _readRecoveryReinitializations{0};
	bool _validateReadCrc{false};
	bool _multiBlockReadEnabled{true};
	bool _multiBlockWriteEnabled{false};
	bool _dmaTransfersEnabled{true};
	bool _highSpeedWritesEnabled{true};
	bool _diagnosticsEnabled{true};
	std::uint32_t _diagnosticSequence{0};
	std::uint32_t _diagnosticWriteIndex{0};
	std::uint32_t _diagnosticStoredCount{0};
	std::uint32_t _diagnosticDroppedCount{0};
	std::array<SdCardDiagnosticEntry, sdCardDiagnosticEntryCount> _diagnosticEntries{};
};

} // namespace lexxos::devices
