#pragma once

#include <lexxos/devices/rk3506/lcd_spi_dma.h>
#include <lexxos/devices/rk3506/picocalc_lcd_spi0.h>

namespace lexxos::platform::rk3506 {

// The product owns one controller for physical SPI0. Callers must hold its
// transaction queue; this class does not add another lock or reset DMA state.
class LcdSpiController {
public:
	explicit LcdSpiController(LcdSpiDma& dma)
	    : _dma(dma) {}

	LcdSpiController(LcdSpiController const&) = delete;
	LcdSpiController& operator=(LcdSpiController const&) = delete;

	bool initialize(std::uint32_t clockHz);
	bool setClockHz(std::uint32_t clockHz);
	std::uint32_t clockHz() const;
	bool setRgb565Direct16(bool enabled);
	bool rgb565Direct16Enabled() const;
	bool write(std::uint8_t const* data, std::size_t count);
	bool writeRgb565Words(std::uint16_t const* pixels, std::size_t count);
	LcdSpi0Statistics statistics() const;

private:
	bool fail();
	bool writeImpl(std::uint8_t const* data, std::size_t count);
	LcdSpiDma& _dma;
	LcdSpi0Statistics _statistics{};
	bool _ready{};
	bool _rgb565Direct16{};
	std::uint32_t _clockHz{};
};

} // namespace lexxos::platform::rk3506
