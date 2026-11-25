#include <sim_i2c_bus.h>

#include <sim_runtime.h>

namespace lexxos::emulator::detail {

bool SimI2cBus::initialize(lexxos::platform::I2cPins const& pins) {
	if (!pins.isValid()) {
		_ready = false;
		return false;
	}

	_bus = static_cast<std::uint8_t>(pins.index);
	_ready = true;
	return true;
}

bool SimI2cBus::isReady() const {
	return _ready;
}

bool SimI2cBus::probe(std::uint8_t address) {
	auto* runtime = activeRuntime();
	return _ready && runtime != nullptr && runtime->i2cProbe(_bus, address);
}

bool SimI2cBus::write(std::uint8_t address, std::uint8_t const* data, std::size_t size) {
	auto* runtime = activeRuntime();
	return _ready && runtime != nullptr && runtime->i2cWrite(_bus, address, data, size);
}

} // namespace lexxos::emulator::detail

lexxos::devices::I2cBus& lexxos::devices::debugI2cBus() {
	static lexxos::emulator::detail::SimI2cBus bus;
	return bus;
}

lexxos::devices::I2cBus& lexxos::devices::systemI2cBus() {
	return lexxos::devices::debugI2cBus();
}
