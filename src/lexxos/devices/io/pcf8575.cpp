#include <lexxos/devices/io/pcf8575.h>

#include <array>

namespace lexxos::devices {

Pcf8575PortExpander::Pcf8575PortExpander(I2cBus& bus, lexxos::platform::Pcf8575Pins config)
    : _bus(bus)
    , _config(config) {
}

bool Pcf8575PortExpander::initialize() {
	if (!_config.isValid() || !_bus.isReady()) {
		_ready = false;
		return false;
	}

	_ready = writePort(0xFFFF);
	return _ready;
}

std::string_view Pcf8575PortExpander::name() const {
	return _config.name != nullptr ? std::string_view{_config.name} : std::string_view{};
}

bool Pcf8575PortExpander::isReady() const {
	return _ready && _bus.isReady();
}

std::uint8_t Pcf8575PortExpander::address() const {
	return _config.address;
}

lexxos::platform::GpioPin Pcf8575PortExpander::interruptPin() const {
	return _config.interrupt;
}

I2cBusTransaction Pcf8575PortExpander::transaction() {
	return I2cBusTransaction{_bus};
}

bool Pcf8575PortExpander::writePort(std::uint16_t value) {
	std::array<std::uint8_t, 2> data{
	    static_cast<std::uint8_t>(value & 0xFFU),
	    static_cast<std::uint8_t>((value >> 8U) & 0xFFU),
	};

	if (!_bus.write(_config.address, data.data(), data.size())) {
		return false;
	}

	_lastWrittenPort = value;
	return true;
}

bool Pcf8575PortExpander::readPort(std::uint16_t& value) {
	std::array<std::uint8_t, 2> data{};
	if (!_bus.read(_config.address, data.data(), data.size())) {
		return false;
	}

	value = static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8U));
	return true;
}

bool Pcf8575PortExpander::releasePins(std::uint16_t mask) {
	return writePort(static_cast<std::uint16_t>(_lastWrittenPort | mask));
}

std::uint16_t Pcf8575PortExpander::lastWrittenPort() const {
	return _lastWrittenPort;
}

} // namespace lexxos::devices
