#include <cstdlib>
#include <iostream>

#include <lexxos/composition/products/lyra_i2c_bus.h>
#include <lexxos/platform/rockchip/rk3506/i2c2_io.h>

namespace {

unsigned hardwareFactories{}, softwareFactories{};

void expect(bool value, char const* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

} // namespace

namespace lexxos::platform::rk3506 {

devices::Rk3506I2cIo hardwareI2c2Io() {
	++hardwareFactories;
	return {};
}

devices::SoftwareI2cIo softwareI2c2Io() {
	++softwareFactories;
	return {};
}

} // namespace lexxos::platform::rk3506

int main() {
	using lexxos::composition::products::lyraSouthbridgeBus;
	using lexxos::devices::debugI2cBus;
	using lexxos::devices::DeviceResourcePriority;
	using lexxos::devices::I2cBusTransaction;
	using lexxos::devices::systemI2cBus;
	// Legacy callers may be first. Both names and composition must still
	// return exactly one object, including its transaction queue.
	auto& bus = systemI2cBus();
	expect(&bus == &debugI2cBus() && &bus == &lyraSouthbridgeBus(), "duplicate bus owner");
	expect(!bus.isReady(), "construction initialized hardware");
#if defined(LEXXOS_RK3506_I2C_SOFTWARE)
	expect(softwareFactories == 1 && hardwareFactories == 0, "wrong software selection/lifetime");
#else
	expect(hardwareFactories == 1 && softwareFactories == 0, "wrong hardware selection/lifetime");
#endif
	{
		I2cBusTransaction first{bus, DeviceResourcePriority::System, 0};
		expect(first.active() && debugI2cBus().transactionBusy(), "alias lost physical bus ownership");
		I2cBusTransaction second{debugI2cBus(), DeviceResourcePriority::System, 0};
		expect(!second.active(), "alias created another arbiter");
	}
	expect(!systemI2cBus().transactionBusy(), "transaction not released through alias");
	I2cBusTransaction next{lyraSouthbridgeBus(), DeviceResourcePriority::System, 0};
	expect(next.active(), "composition cannot reacquire released bus");
}
