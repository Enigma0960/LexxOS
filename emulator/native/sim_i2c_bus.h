#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::emulator::detail {

class SimI2cBus final : public lexxos::devices::I2cBus {
public:
	bool initialize(lexxos::platform::I2cPins const& pins) override;
	[[nodiscard]] bool isReady() const override;
	bool probe(std::uint8_t address) override;
	bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) override;

private:
	std::uint8_t _bus{0};
	bool _ready{false};
};

} // namespace lexxos::emulator::detail
