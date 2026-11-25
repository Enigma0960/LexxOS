#pragma once
#include <lexxos/devices/storage/sd_card_backend.h>
#include <lexxos/devices/storage/sd_media_transport.h>

namespace lexxos::devices::rk3506 {

class PicoSdBlockDevice final : public SdCardBackend {
public:
	PicoSdBlockDevice(SdMediaTransport& transport, bool& writeArmed, void (*writeFailed)());
	PicoSdBlockDevice(SdMediaTransport& transport, bool& writeArmed, void (*writeFailed)(void*), void* context);
	bool initialize() override;
	bool isReady() const override;
	bool isReadOnly() const override;
	std::uint32_t blockSizeBytes() const override;
	std::uint64_t blockCount() const override;
	bool readBlocks(std::uint64_t first, void* destination, std::size_t count) override;
	bool writeBlocks(std::uint64_t first, void const* source, std::size_t count) override;

	// Generic SD API over the existing native SPI1 transport.
	SdCardInterface interfaceType() const override;
	SdCardState state() const override;
	SdCardType cardType() const override;
	std::uint32_t activeClockHz() const override;
	SdTimingProfile timingProfile() const override;
	bool setTransferBaudRate(std::uint32_t baudRate) override;
	bool setSpiClockMode(SpiClockMode mode) override;
	SpiClockMode spiClockMode() const override;
	bool readCrcValidation() const override;
	bool multiBlockReadEnabled() const override;
	bool dmaSupported() const override;
	bool dmaTransfersEnabled() const override;
	bool hasCardDetect() const override;
	bool isCardInserted() const override;

private:
	SdMediaTransport& _transport;
	bool& _writeArmed;
	void (*_writeFailed)(){nullptr};
	void (*_contextWriteFailed)(void*){nullptr};
	void* _failureContext{nullptr};
};

} // namespace lexxos::devices::rk3506
