#include <lexxos/platform/espidf/detail/espidf_gpio.h>

void lexxos::platform::espidf::detail::initializeStatusLed() {
	auto const& descriptor = activeBoardDescriptor();
	if (!descriptor.statusLed.isValid()) {
		return;
	}
	configureGpioOutput(descriptor.statusLed, false);
}

void lexxos::platform::setStatusLed(bool enabled) {
	auto const& descriptor = activeBoardDescriptor();
	if (!descriptor.statusLed.isValid()) {
		return;
	}

	auto const active = descriptor.statusLedActiveLevel == GpioActiveLevel::High;
	writeGpio(descriptor.statusLed, enabled ? active : !active);
}
