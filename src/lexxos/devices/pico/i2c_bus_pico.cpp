#include <lexxos/devices/pico/pico_i2c_bus.h>

namespace lexxos::devices {

I2cBus& debugI2cBus() {
	static PicoI2cBus bus;
	return bus;
}

I2cBus& systemI2cBus() {
	return debugI2cBus();
}

} // namespace lexxos::devices
