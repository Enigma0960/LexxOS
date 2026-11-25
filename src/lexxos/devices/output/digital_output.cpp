#include <lexxos/devices/output/digital_output.h>

namespace lexxos::devices {

DigitalOutput::DigitalOutput(platform::PlatformGpioService const& gpio)
    : _gpio(gpio) {
}

bool DigitalOutput::initialize(platform::GpioPin pin, bool initialHigh) {
	_pin = {};
	_state = DigitalOutputState::Unknown;
	if (!pin.isValid() || !_gpio.configureOutput || !_gpio.write || !_gpio.configureOutput(pin, initialHigh)) {
		return false;
	}

	_pin = pin;
	_state = initialHigh ? DigitalOutputState::High : DigitalOutputState::Low;
	return true;
}

bool DigitalOutput::write(bool high) {
	if (!_pin.isValid()) {
		return false;
	}

	if (!_gpio.write(_pin, high)) {
		_state = DigitalOutputState::Unknown;
		return false;
	}

	_state = high ? DigitalOutputState::High : DigitalOutputState::Low;
	return true;
}

bool DigitalOutput::toggle() {
	return _state != DigitalOutputState::Unknown && write(_state == DigitalOutputState::Low);
}

} // namespace lexxos::devices
