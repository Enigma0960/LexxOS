#include <lexxos/devices/stm32/stm32_placeholder_i2c_bus.h>

namespace lexxos::devices {

bool Stm32PlaceholderI2cBus::initialize(lexxos::platform::I2cPins const&) {
	return false;
}

bool Stm32PlaceholderI2cBus::isReady() const {
	return false;
}

bool Stm32PlaceholderI2cBus::probe(std::uint8_t) {
	return false;
}

bool Stm32PlaceholderI2cBus::write(std::uint8_t, std::uint8_t const*, std::size_t) {
	return false;
}

bool Stm32PlaceholderI2cBus::read(std::uint8_t, std::uint8_t*, std::size_t) {
	return false;
}

} // namespace lexxos::devices
