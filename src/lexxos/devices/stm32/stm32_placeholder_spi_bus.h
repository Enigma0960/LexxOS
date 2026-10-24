#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/bus/spi_bus.h>

namespace lexxos::devices {

class Stm32PlaceholderSpiBus final : public SpiBus {
public:
	bool initialize(lexxos::platform::SpiPins const& pins) override;
	[[nodiscard]] bool isReady() const override;
	bool setBaudRate(std::uint32_t baudRate) override;
	bool setClockMode(SpiClockMode mode) override;
	[[nodiscard]] SpiClockMode clockMode() const override;
	void select() override;
	void deselect() override;
	bool transfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) override;
	bool write(std::uint8_t const* data, std::size_t size) override;
	bool read(std::uint8_t fill, std::uint8_t* data, std::size_t size) override;
};

} // namespace lexxos::devices
