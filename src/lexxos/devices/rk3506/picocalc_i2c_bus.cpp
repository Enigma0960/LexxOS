// Compatibility aliases; the controller is owned by product composition.
#include <lexxos/composition/products/lyra_i2c_bus.h>
#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::devices {

I2cBus& debugI2cBus() {
	return composition::products::lyraSouthbridgeBus();
}

I2cBus& systemI2cBus() {
	return composition::products::lyraSouthbridgeBus();
}

} // namespace lexxos::devices
