#include <lexxos/devices/espidf/espidf_spi_bus.h>

namespace lexxos::devices {

SpiBus& sdCardSpiBus() {
	static EspIdfSpiBus bus;
	return bus;
}

SpiBus& displaySpiBus() {
	static EspIdfSpiBus bus;
	return bus;
}

} // namespace lexxos::devices
