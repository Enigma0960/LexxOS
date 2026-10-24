#include <lexxos/devices/host/host_spi_bus.h>

namespace lexxos::devices {

SpiBus& sdCardSpiBus() {
	static HostSpiBus bus;
	return bus;
}

} // namespace lexxos::devices
