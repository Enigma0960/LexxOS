#include <lexxos/devices/stm32/stm32_placeholder_spi_bus.h>

namespace lexxos::devices {

bool Stm32PlaceholderSpiBus::initialize(lexxos::platform::SpiPins const&) {
	return false;
}

bool Stm32PlaceholderSpiBus::isReady() const {
	return false;
}

bool Stm32PlaceholderSpiBus::setBaudRate(std::uint32_t) {
	return false;
}

bool Stm32PlaceholderSpiBus::setClockMode(SpiClockMode) {
	return false;
}

SpiClockMode Stm32PlaceholderSpiBus::clockMode() const {
	return SpiClockMode::Mode0;
}

void Stm32PlaceholderSpiBus::select() {
}

void Stm32PlaceholderSpiBus::deselect() {
}

bool Stm32PlaceholderSpiBus::transfer(std::uint8_t const*, std::uint8_t*, std::size_t) {
	return false;
}

bool Stm32PlaceholderSpiBus::write(std::uint8_t const*, std::size_t) {
	return false;
}

bool Stm32PlaceholderSpiBus::read(std::uint8_t, std::uint8_t*, std::size_t) {
	return false;
}

} // namespace lexxos::devices
