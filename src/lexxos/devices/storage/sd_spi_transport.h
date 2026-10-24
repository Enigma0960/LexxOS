#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/platform/board.h>

namespace lexxos::devices {

class SdSpiTransport {
public:
	SdSpiTransport(SpiBus& bus, lexxos::platform::SpiPins pins);

	[[nodiscard]] lexxos::platform::SpiPins const& pins() const;
	[[nodiscard]] std::uint32_t configuredBaudRate() const;

	bool initialize();
	[[nodiscard]] bool isReady() const;
	bool setBaudRate(std::uint32_t baudRate);
	bool setClockMode(SpiClockMode mode);
	[[nodiscard]] SpiClockMode clockMode() const;
	void select();
	void deselect();
	bool write(std::uint8_t const* data, std::size_t size);
	bool read(std::uint8_t fill, std::uint8_t* data, std::size_t size);
	[[nodiscard]] bool supportsDma() const;
	bool startDmaTransfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size);
	bool waitDmaTransfer(std::uint32_t timeoutMs);

private:
	SpiBus& _bus;
	lexxos::platform::SpiPins _pins;
};

} // namespace lexxos::devices
