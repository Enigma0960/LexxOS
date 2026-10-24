#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/storage/sd_card_backend.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

class SdSdioBackend final : public SdCardBackend {
public:
	static constexpr std::uint32_t blockSize = 512;

	SdSdioBackend(lexxos::platform::SdioPins pins, lexxos::platform::GpioCardDetect cardDetect = {});
	SdSdioBackend(
	    lexxos::platform::SdioPins pins,
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
	[[nodiscard]] std::uint32_t activeClockHz() const override;
	[[nodiscard]] SdTimingProfile timingProfile() const override;
	[[nodiscard]] bool hasCardDetect() const override;
	[[nodiscard]] bool isCardInserted() const override;
	bool setTransferBaudRate(std::uint32_t baudRate) override;
	void setDiagnosticsEnabled(bool enabled) override;
	[[nodiscard]] bool diagnosticsEnabled() const override;
	void clearDiagnostics() override;
	[[nodiscard]] SdCardDiagnostics diagnostics() override;

private:
	enum class ResponseType : std::uint8_t {
		None,
		Short,
		ShortNoCrc,
		Long,
	};

	struct CommandResult {
		bool ok{false};
		std::uint32_t response[4]{};
	};

	bool configureCardDetect();
	[[nodiscard]] bool physicalCardPresent() const;
	bool configurePeripheral();
	bool setClock(std::uint32_t clockHz);
	CommandResult sendCommand(std::uint8_t command, std::uint32_t argument, ResponseType responseType);
	CommandResult sendApplicationCommand(std::uint8_t command, std::uint32_t argument, ResponseType responseType);
	bool selectCard();
	bool configureBusWidth();
	bool readCsd();
	bool readSingleBlock(std::uint64_t block, std::uint8_t* buffer);
	bool writeSingleBlock(std::uint64_t block, std::uint8_t const* buffer);
	void recordDiagnostic(
	    SdCardDiagnosticCode code,
	    std::uint32_t command = 0xFFU,
	    std::uint32_t r1 = 0xFFFFFFFFU,
	    std::uint64_t block = 0,
	    std::uint32_t blockCount = 0);
	[[nodiscard]] SdCardDiagnostics diagnosticsUnlocked() const;
	void resetState(SdCardState state = SdCardState::Uninitialized);
	void setFailure(SdCardState state);
	[[nodiscard]] std::uint32_t uptimeMs() const;
	void sleepMs(std::uint32_t milliseconds) const;
	[[nodiscard]] std::uint32_t commandAddress(std::uint64_t block) const;

	lexxos::platform::PlatformServices const& _platformServices;
	lexxos::platform::SdioPins _pins;
	lexxos::platform::GpioCardDetect _cardDetect;
	SdCardState _state{SdCardState::Uninitialized};
	SdCardType _cardType{SdCardType::Unknown};
	std::uint16_t _rca{0};
	std::uint64_t _blockCount{0};
	std::uint32_t _activeClockHz{0};
	bool _diagnosticsEnabled{true};
	std::uint32_t _diagnosticSequence{0};
	std::uint32_t _diagnosticWriteIndex{0};
	std::uint32_t _diagnosticStoredCount{0};
	std::uint32_t _diagnosticDroppedCount{0};
	std::array<SdCardDiagnosticEntry, sdCardDiagnosticEntryCount> _diagnosticEntries{};
};

} // namespace lexxos::devices
