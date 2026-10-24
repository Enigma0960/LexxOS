#include <lexxos/devices/stm32/stm32_placeholder_i2c_bus.h>

namespace lexxos::devices {

I2cBus& debugI2cBus() {
	static Stm32PlaceholderI2cBus bus;
	return bus;
}

I2cBus& systemI2cBus() {
	return debugI2cBus();
}

} // namespace lexxos::devices
