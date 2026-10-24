#include <lexxos/devices/host/host_spi_bus.h>

namespace lexxos::devices {

bool HostSpiBus::initialize(lexxos::platform::SpiPins const&) {
	return false;
}

bool HostSpiBus::isReady() const {
	return false;
}

bool HostSpiBus::setBaudRate(std::uint32_t) {
	return false;
}

bool HostSpiBus::setClockMode(SpiClockMode) {
	return false;
}

SpiClockMode HostSpiBus::clockMode() const {
	return SpiClockMode::Mode0;
}

void HostSpiBus::select() {
}

void HostSpiBus::deselect() {
}

bool HostSpiBus::transfer(std::uint8_t const*, std::uint8_t*, std::size_t) {
	return false;
}

bool HostSpiBus::write(std::uint8_t const*, std::size_t) {
	return false;
}

bool HostSpiBus::read(std::uint8_t, std::uint8_t*, std::size_t) {
	return false;
}

} // namespace lexxos::devices
