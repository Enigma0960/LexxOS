#include <lexxos/devices/rk3506/native_sd_protocol_io.h>

#include <lexxos/devices/rk3506/sd_dma_controller.h>
#include <lexxos/devices/rk3506/sd_spi_controller.h>
#include <lexxos/platform/rockchip/rk3506/sd_gpio.h>

extern "C" std::uint32_t rk3506_timer_ticks();
extern "C" std::uint64_t rk3506_timer_counter();

namespace lexxos::platform::rk3506 {

NativeSdProtocolIo::NativeSdProtocolIo(SdSpiController& spi, SdDmaController& dma)
    : _spi(spi)
    , _dma(dma) {
}

void NativeSdProtocolIo::gpioConfigure() {
	sdGpioConfigure();
}

void NativeSdProtocolIo::gpioChipSelect(bool high) {
	sdGpioChipSelect(high);
}

std::uint8_t NativeSdProtocolIo::gpioTransfer(std::uint8_t output) {
	return sdGpioTransfer(output);
}

void NativeSdProtocolIo::gpioEnableClock() {
	sdGpioEnableClock();
}

bool NativeSdProtocolIo::gpioCardInserted() {
	return sdGpioCardInserted();
}

PicoSdRegisterStatus NativeSdProtocolIo::gpioRegisters(bool configured) {
	auto const snapshot = sdGpioRegisters(configured);
	return {snapshot.gpioGate, snapshot.spiGate, snapshot.iocC, snapshot.iocD,
	    snapshot.detectReadable, snapshot.detectHigh};
}

bool NativeSdProtocolIo::spiInitialize() {
	return _spi.initialize();
}

void NativeSdProtocolIo::spiStop() {
	_spi.stop();
}

bool NativeSdProtocolIo::spiTransfer(std::uint8_t output, std::uint8_t& input) {
	return _spi.transfer(output, input);
}

bool NativeSdProtocolIo::spiReadData(std::uint8_t* destination, std::size_t count) {
	return _spi.readData(destination, count);
}

void NativeSdProtocolIo::spiFastClock() {
	_spi.fastClock();
}

bool NativeSdProtocolIo::spiSetClockMHz(std::uint32_t mhz) {
	return _spi.setClockMHz(mhz);
}

void NativeSdProtocolIo::dmaReset() {
	_dma.reset();
}

bool NativeSdProtocolIo::dmaEnabled() {
	return _dma.enabled();
}

bool NativeSdProtocolIo::dmaProbe() {
	return _dma.probe();
}

bool NativeSdProtocolIo::dmaReadData(std::uint8_t* destination, std::size_t count) {
	return _dma.readData(destination, count);
}

bool NativeSdProtocolIo::dmaSetDirect(bool enabled) {
	return _dma.setDirect(enabled);
}

bool NativeSdProtocolIo::dmaSetEnabled(bool enabled) {
	return _dma.setEnabled(enabled);
}

std::uint32_t NativeSdProtocolIo::ticks() {
	return rk3506_timer_ticks();
}

std::uint64_t NativeSdProtocolIo::counter() {
	return rk3506_timer_counter();
}

} // namespace lexxos::platform::rk3506
