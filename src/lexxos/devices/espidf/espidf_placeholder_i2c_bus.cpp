#include <lexxos/devices/espidf/espidf_placeholder_i2c_bus.h>

namespace lexxos::devices {

bool EspIdfPlaceholderI2cBus::initialize(lexxos::platform::I2cPins const&) {
	return false;
}

bool EspIdfPlaceholderI2cBus::isReady() const {
	return false;
}

bool EspIdfPlaceholderI2cBus::probe(std::uint8_t) {
	return false;
}

bool EspIdfPlaceholderI2cBus::write(std::uint8_t, std::uint8_t const*, std::size_t) {
	return false;
}

bool EspIdfPlaceholderI2cBus::read(std::uint8_t, std::uint8_t*, std::size_t) {
	return false;
}

} // namespace lexxos::devices
