#include <lexxos/devices/host/host_i2c_bus.h>

namespace lexxos::devices {

bool HostI2cBus::initialize(lexxos::platform::I2cPins const&) {
	return false;
}

bool HostI2cBus::isReady() const {
	return false;
}

bool HostI2cBus::probe(std::uint8_t) {
	return false;
}

bool HostI2cBus::write(std::uint8_t, std::uint8_t const*, std::size_t) {
	return false;
}

bool HostI2cBus::read(std::uint8_t, std::uint8_t*, std::size_t) {
	return false;
}

} // namespace lexxos::devices
