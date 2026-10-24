#include <lexxos/platform/pico/detail/pico_gpio.h>

#include <hardware/gpio.h>

#include <lexxos/platform/pico/detail/pico_gpio_interrupts.h>
#include <lexxos/platform/platform.h>

namespace {

uint toPicoGpioPin(lexxos::platform::GpioPin pin) {
	return static_cast<uint>(pin.number);
}

} // namespace

void lexxos::platform::pico::detail::initializeStatusLed() {
	auto const pin = activeBoardDescriptor().statusLed;
	if (!pin.isValid()) {
		return;
	}

	gpio_init(toPicoGpioPin(pin));
	gpio_set_dir(toPicoGpioPin(pin), GPIO_OUT);
	gpio_put(toPicoGpioPin(pin), 1);
}

void lexxos::platform::setStatusLed(bool enabled) {
	auto const pin = activeBoardDescriptor().statusLed;
	if (pin.isValid()) {
		gpio_put(toPicoGpioPin(pin), enabled ? 1 : 0);
	}
}

bool lexxos::platform::configureGpioInput(GpioPin pin, GpioPull pull) {
	if (!pin.isValid()) {
		return false;
	}

	uint const gpio = toPicoGpioPin(pin);
	gpio_init(gpio);
	gpio_set_dir(gpio, GPIO_IN);
	switch (pull) {
		case GpioPull::None:
			gpio_disable_pulls(gpio);
			break;
		case GpioPull::Up:
			gpio_pull_up(gpio);
			break;
		case GpioPull::Down:
			gpio_pull_down(gpio);
			break;
	}
	return true;
}

bool lexxos::platform::configureGpioOutput(GpioPin pin, bool initialValue) {
	if (!pin.isValid()) {
		return false;
	}

	uint const gpio = toPicoGpioPin(pin);
	gpio_init(gpio);
	gpio_set_dir(gpio, GPIO_OUT);
	gpio_put(gpio, initialValue ? 1 : 0);
	return true;
}

bool lexxos::platform::readGpio(GpioPin pin, bool& value) {
	if (!pin.isValid()) {
		return false;
	}

	value = gpio_get(toPicoGpioPin(pin)) != 0;
	return true;
}

bool lexxos::platform::writeGpio(GpioPin pin, bool value) {
	if (!pin.isValid()) {
		return false;
	}

	gpio_put(toPicoGpioPin(pin), value ? 1 : 0);
	return true;
}

bool lexxos::platform::configureGpioInterrupt(
    GpioPin pin,
    GpioInterruptEdge edge,
    GpioInterruptHandler handler,
    void* userData) {
	return pico::detail::configureGpioInterrupt(pin, edge, handler, userData);
}

void lexxos::platform::disableGpioInterrupt(GpioPin pin) {
	pico::detail::disableGpioInterrupt(pin);
}
