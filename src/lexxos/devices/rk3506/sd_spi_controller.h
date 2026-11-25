#pragma once

#include <lexxos/devices/rk3506/picosd_spi1_hw.h>

namespace lexxos::platform::rk3506 {

// One product-owned instance for physical SPI1. Callers retain SPI1 queue/CS
// ownership; SD protocol, DMA capability and media lifecycle remain separate.
class SdSpiController {
public:
	SdSpiController() = default;
	SdSpiController(SdSpiController const&) = delete;
	SdSpiController& operator=(SdSpiController const&) = delete;

	bool initialize();
	void stop();
	bool transfer(std::uint8_t tx, std::uint8_t& rx);
	bool readData(std::uint8_t* destination, std::size_t count);
	void fastClock();
	bool setClockMHz(std::uint32_t mhz);
	std::uint32_t clockMHz() const;
	std::uint32_t version() const;
	std::uint32_t divider() const;
	PicoSpi1FifoStats fifoStatistics() const;
	PicoSpi1FifoMode fifoMode() const;
	bool setFifoMode(PicoSpi1FifoMode mode);
	bool resetFifoStatistics();

private:
	bool abortFifoRead(bool timeout, bool overflow);
	bool _initialized{};
	PicoSpi1FifoStats _fifoStats{};
	PicoSpi1FifoMode _fifoMode{PicoSpi1FifoMode::Safe};
};

} // namespace lexxos::platform::rk3506
