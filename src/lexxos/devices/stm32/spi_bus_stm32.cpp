#include <lexxos/devices/stm32/stm32_placeholder_spi_bus.h>

namespace lexxos::devices {

SpiBus& sdCardSpiBus() {
	static Stm32PlaceholderSpiBus bus;
	return bus;
}

} // namespace lexxos::devices
