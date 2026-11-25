#pragma once

#include <lexxos/devices/rk3506/sd_protocol_io.h>

namespace lexxos::platform::rk3506 {

class SdSpiController;
class SdDmaController;

// Native bridge only; protocol/media state belongs to SdProtocol.
class NativeSdProtocolIo final : public SdProtocolIo {
public:
	NativeSdProtocolIo(SdSpiController& spi, SdDmaController& dma);
	void gpioConfigure() override;
	void gpioChipSelect(bool high) override;
	std::uint8_t gpioTransfer(std::uint8_t output) override;
	void gpioEnableClock() override;
	bool gpioCardInserted() override;
	PicoSdRegisterStatus gpioRegisters(bool configured) override;
	bool spiInitialize() override;
	void spiStop() override;
	bool spiTransfer(std::uint8_t output, std::uint8_t& input) override;
	bool spiReadData(std::uint8_t* destination, std::size_t count) override;
	void spiFastClock() override;
	bool spiSetClockMHz(std::uint32_t mhz) override;
	void dmaReset() override;
	bool dmaEnabled() override;
	bool dmaProbe() override;
	bool dmaReadData(std::uint8_t* destination, std::size_t count) override;
	bool dmaSetDirect(bool enabled) override;
	bool dmaSetEnabled(bool enabled) override;
	std::uint32_t ticks() override;
	std::uint64_t counter() override;

private:
	SdSpiController& _spi;
	SdDmaController& _dma;
};

} // namespace lexxos::platform::rk3506
