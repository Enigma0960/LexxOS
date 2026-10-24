#include <lexxos/devices/pico/pico_spi_bus.h>

namespace lexxos::devices {

SpiBus& sdCardSpiBus() {
	static PicoSpiBus bus;
	return bus;
}

} // namespace lexxos::devices
