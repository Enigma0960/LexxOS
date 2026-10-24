#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::devices {

class Stm32PlaceholderI2cBus final : public I2cBus {
public:
	bool initialize(lexxos::platform::I2cPins const& pins) override;
	[[nodiscard]] bool isReady() const override;
	bool probe(std::uint8_t address) override;
	bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) override;
	bool read(std::uint8_t address, std::uint8_t* data, std::size_t size) override;
};

} // namespace lexxos::devices
