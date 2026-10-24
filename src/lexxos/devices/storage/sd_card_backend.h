#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/devices/storage/block_device.h>

namespace lexxos::devices {

enum class SdCardInterface : std::uint8_t {
	Unknown,
	Spi,
	Sdio,
	Simulated,
};

enum class SdCardState : std::uint8_t {
	Uninitialized,
	Ready,
	NoBus,
	NoCard,
	Unsupported,
	Timeout,
	IoError,
	CardRemoved,
};

enum class SdCardType : std::uint8_t {
	Unknown,
	SdV1,
	SdV2ByteAddressed,
	SdV2BlockAddressed,
};

struct SdTimingProfile {
	std::uint32_t initializationClockHz{0};
	std::uint32_t safeClockHz{0};
	std::uint32_t defaultClockHz{0};
	std::uint32_t performanceClockHz{0};
	std::uint32_t maxClockHz{0};
};

static constexpr std::size_t sdCardDiagnosticEntryCount = 16;

enum class SdCardDiagnosticCode : std::uint8_t {
	None,
	LockFailed,
	InitializeFailed,
	SdioPowerControlFailed,
	SdioHostInitFailed,
	SdioSlotInitFailed,
	SdioCardInitFailed,
	NotReady,
	NullBuffer,
	CardRemoved,
	SetBaudFailed,
	SetModeFailed,
	WaitReadyTimeout,
	CommandWriteFailed,
	CommandResponseTimeout,
	CommandResponseReadFailed,
	CommandExtraReadFailed,
	RegisterTokenTimeout,
	RegisterReadFailed,
	DataTokenTimeout,
	DmaStartFailed,
	DmaWaitTimeout,
	CrcReadFailed,
	ReadCrcMismatch,
	Cmd17Rejected,
	Cmd18Rejected,
	Cmd12WriteFailed,
	Cmd12ResponseReadFailed,
	Cmd12Rejected,
	Cmd12BusyTimeout,
	WriteNotAllowed,
	WriteSpeedGuard,
	Cmd24Rejected,
	WriteTokenFailed,
	WriteDmaStartFailed,
	WriteDmaWaitFailed,
	WriteCrcWriteFailed,
	WriteResponseReadFailed,
	WriteDataRejected,
	WriteBusyTimeout,
	Cmd25Rejected,
	MultiWriteStopFailed,
	Cmd13Rejected,
	Cmd13StatusError,
	ReadFallbackToSafeBaud,
	WriteFallbackToSafeBaud,
	ReinitializeFailed,
	TransferFailure,
};

struct SdCardDiagnosticEntry {
	std::uint32_t sequence{0};
	std::uint32_t uptimeMs{0};
	SdCardDiagnosticCode code{SdCardDiagnosticCode::None};
	SdCardState state{SdCardState::Uninitialized};
	SdCardType cardType{SdCardType::Unknown};
	SpiClockMode mode{SpiClockMode::Mode0};
	std::uint32_t activeBaudRate{0};
	std::uint32_t command{0xFFU};
	std::uint32_t r1{0xFFFFFFFFU};
	std::uint64_t block{0};
	std::uint32_t blockCount{0};
};

struct SdCardDiagnostics {
	bool enabled{false};
	std::uint32_t sequence{0};
	std::uint32_t storedCount{0};
	std::uint32_t droppedCount{0};
	std::array<SdCardDiagnosticEntry, sdCardDiagnosticEntryCount> entries{};
};

[[nodiscard]] char const* toString(SdCardInterface interfaceType);
[[nodiscard]] char const* toString(SdCardDiagnosticCode code);
[[nodiscard]] char const* toString(SdCardState state);
[[nodiscard]] char const* toString(SdCardType type);

class SdCardBackend : public BlockDevice {
public:
	~SdCardBackend() override = default;

	[[nodiscard]] virtual SdCardInterface interfaceType() const = 0;
	[[nodiscard]] virtual SdCardState state() const = 0;
	[[nodiscard]] virtual SdCardType cardType() const = 0;
	[[nodiscard]] virtual std::uint32_t activeClockHz() const = 0;
	[[nodiscard]] virtual SdTimingProfile timingProfile() const;
	[[nodiscard]] virtual bool hasCardDetect() const = 0;
	[[nodiscard]] virtual bool isCardInserted() const = 0;

	virtual bool setTransferBaudRate(std::uint32_t baudRate);
	virtual bool setSpiClockMode(SpiClockMode mode);
	[[nodiscard]] virtual SpiClockMode spiClockMode() const;
	virtual void setReadCrcValidation(bool enabled);
	[[nodiscard]] virtual bool readCrcValidation() const;
	virtual void setMultiBlockRead(bool enabled);
	[[nodiscard]] virtual bool multiBlockReadEnabled() const;
	virtual void setMultiBlockWrite(bool enabled);
	[[nodiscard]] virtual bool multiBlockWriteEnabled() const;
	[[nodiscard]] virtual bool dmaSupported() const;
	virtual void setDmaTransfersEnabled(bool enabled);
	[[nodiscard]] virtual bool dmaTransfersEnabled() const;
	virtual void setHighSpeedWritesEnabled(bool enabled);
	[[nodiscard]] virtual bool highSpeedWritesEnabled() const;
	virtual void setDiagnosticsEnabled(bool enabled);
	[[nodiscard]] virtual bool diagnosticsEnabled() const;
	virtual void clearDiagnostics();
	[[nodiscard]] virtual SdCardDiagnostics diagnostics();
	[[nodiscard]] virtual std::uint32_t readRecoveryReinitializations() const;
};

} // namespace lexxos::devices
