#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/rk3506/picosd_diagnostics.h>

namespace lexxos::platform::rk3506 {

// Borrowed synchronous I/O for one socket; must outlive SdProtocol.
// Caller retains SPI1 ownership. ticks() is wrapping milliseconds; counter()
// is the monotonic hardware cycle counter used for profiling.
// Data buffers are released by readData before return, including failure.
class SdProtocolIo {
public:
	virtual ~SdProtocolIo() = default;
	virtual void gpioConfigure() = 0;
	virtual void gpioChipSelect(bool high) = 0;
	virtual std::uint8_t gpioTransfer(std::uint8_t output) = 0;
	virtual void gpioEnableClock() = 0;
	virtual bool gpioCardInserted() = 0;
	virtual PicoSdRegisterStatus gpioRegisters(bool configured) = 0;
	virtual bool spiInitialize() = 0;
	virtual void spiStop() = 0;
	virtual bool spiTransfer(std::uint8_t output, std::uint8_t& input) = 0;
	virtual bool spiReadData(std::uint8_t* destination, std::size_t count) = 0;
	virtual void spiFastClock() = 0;
	virtual bool spiSetClockMHz(std::uint32_t mhz) = 0;
	virtual void dmaReset() = 0;
	virtual bool dmaEnabled() = 0;
	virtual bool dmaProbe() = 0;
	virtual bool dmaReadData(std::uint8_t* destination, std::size_t count) = 0;
	virtual bool dmaSetDirect(bool enabled) = 0;
	virtual bool dmaSetEnabled(bool enabled) = 0;
	virtual std::uint32_t ticks() = 0;
	virtual std::uint64_t counter() = 0;
};

} // namespace lexxos::platform::rk3506
