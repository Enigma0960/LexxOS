#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::devices {

I2cFailureSnapshot I2cBus::lastFailure() const {
	return {};
}

bool I2cBus::read(std::uint8_t address, std::uint8_t* data, std::size_t size) {
	(void) address;
	(void) data;
	(void) size;
	return false;
}

bool I2cBus::writeRead(std::uint8_t, std::uint8_t const*, std::size_t, std::uint8_t*, std::size_t) {
	return false;
}

bool I2cBus::beginTransaction() {
	return true;
}

void I2cBus::endTransaction() {
}

} // namespace lexxos::devices
