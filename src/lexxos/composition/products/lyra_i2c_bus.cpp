#include <lexxos/composition/products/lyra_i2c_bus.h>

#include <lexxos/platform/rockchip/rk3506/i2c2_io.h>

namespace lexxos::composition::products {

devices::I2cBus& lyraSouthbridgeBus() {
#if defined(LEXXOS_RK3506_I2C_SOFTWARE)
	static devices::SoftwareI2cBus bus{platform::rk3506::softwareI2c2Io()};
#else
	static devices::Rk3506I2cBus bus{platform::rk3506::hardwareI2c2Io()};
#endif
	return bus;
}

} // namespace lexxos::composition::products
