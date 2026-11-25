// Compatibility entrypoints use the single product-owned SPI1 controller.
#include <lexxos/devices/rk3506/picosd_spi1_hw.h>

#include <lexxos/composition/products/lyra_sd_spi.h>
#include <lexxos/devices/rk3506/picosd_dma_pl330.h>
#include <lexxos/devices/rk3506/sd_spi_controller.h>

namespace lexxos::platform::rk3506 {

bool picoSpi1Initialize() {
	return composition::products::lyraSdSpi().initialize();
}

void picoSpi1Stop() {
	composition::products::lyraSdSpi().stop();
}

bool picoSpi1Transfer(std::uint8_t tx, std::uint8_t& rx) {
	return composition::products::lyraSdSpi().transfer(tx, rx);
}

bool picoSpi1ReadData(std::uint8_t* data, std::size_t count) {
	return composition::products::lyraSdSpi().readData(data, count);
}

void picoSpi1FastClock() {
	composition::products::lyraSdSpi().fastClock();
}

bool picoSpi1SetClockMHz(std::uint32_t mhz) {
	return composition::products::lyraSdSpi().setClockMHz(mhz);
}

std::uint32_t picoSpi1ClockMHz() {
	return composition::products::lyraSdSpi().clockMHz();
}

std::uint32_t picoSpi1Version() {
	return composition::products::lyraSdSpi().version();
}

std::uint32_t picoSpi1Divider() {
	return composition::products::lyraSdSpi().divider();
}

PicoSpi1FifoStats picoSpi1FifoStatistics() {
	return composition::products::lyraSdSpi().fifoStatistics();
}

PicoSpi1FifoMode picoSpi1FifoMode() {
	return composition::products::lyraSdSpi().fifoMode();
}

bool picoSpi1SetFifoMode(PicoSpi1FifoMode mode) {
	return composition::products::lyraSdSpi().setFifoMode(mode);
}

bool picoSpi1ResetFifoStatistics() {
	return composition::products::lyraSdSpi().resetFifoStatistics();
}

// DMA capability remains separate from the PIO controller's state.
PicoSpi1DmaInfo picoSpi1DmaInfo() {
	return picoSdDmaInfo();
}

} // namespace lexxos::platform::rk3506
