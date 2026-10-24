#include <lexxos/platform/espidf/detail/espidf_gpio.h>

#include <array>
#include <cstdint>

#include <driver/gpio.h>
#include <esp_attr.h>

#include <lexxos/platform/platform.h>

namespace {

#if defined(CONFIG_IDF_TARGET_ESP32S3)
constexpr int maxGpioPins = 49;
#else
constexpr int maxGpioPins = 40;
#endif

std::array<lexxos::platform::espidf::detail::GpioInterruptSlot, maxGpioPins> g_interrupts{};
bool g_isrServiceInstalled{false};

bool isValidPin(lexxos::platform::GpioPin pin) {
	return pin.number >= 0 && pin.number < maxGpioPins;
}

gpio_num_t toEspGpio(lexxos::platform::GpioPin pin) {
	return static_cast<gpio_num_t>(pin.number);
}

gpio_pullup_t toPullUp(lexxos::platform::GpioPull pull) {
	return pull == lexxos::platform::GpioPull::Up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
}

gpio_pulldown_t toPullDown(lexxos::platform::GpioPull pull) {
	return pull == lexxos::platform::GpioPull::Down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
}

gpio_int_type_t toInterruptType(lexxos::platform::GpioInterruptEdge edge) {
	switch (edge) {
		case lexxos::platform::GpioInterruptEdge::Rising:
			return GPIO_INTR_POSEDGE;
		case lexxos::platform::GpioInterruptEdge::Falling:
			return GPIO_INTR_NEGEDGE;
		case lexxos::platform::GpioInterruptEdge::Both:
			return GPIO_INTR_ANYEDGE;
	}
	return GPIO_INTR_DISABLE;
}

void IRAM_ATTR gpioInterruptTrampoline(void* argument) {
	auto const pinNumber = static_cast<int>(reinterpret_cast<std::intptr_t>(argument));
	if (pinNumber < 0 || pinNumber >= maxGpioPins) {
		return;
	}

	auto const& slot = g_interrupts[pinNumber];
	if (slot.handler == nullptr) {
		return;
	}

	auto const value = gpio_get_level(static_cast<gpio_num_t>(pinNumber)) != 0;
	slot.handler(lexxos::platform::GpioPin{pinNumber}, value, slot.userData);
}

} // namespace

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

bool lexxos::platform::configureGpioInput(GpioPin pin, GpioPull pull) {
	if (!isValidPin(pin)) {
		return false;
	}

	gpio_config_t config{};
	config.pin_bit_mask = 1ULL << static_cast<unsigned>(pin.number);
	config.mode = GPIO_MODE_INPUT;
	config.pull_up_en = toPullUp(pull);
	config.pull_down_en = toPullDown(pull);
	config.intr_type = GPIO_INTR_DISABLE;
	return gpio_config(&config) == ESP_OK;
}

bool lexxos::platform::configureGpioOutput(GpioPin pin, bool initialValue) {
	if (!isValidPin(pin)) {
		return false;
	}

	gpio_config_t config{};
	config.pin_bit_mask = 1ULL << static_cast<unsigned>(pin.number);
	config.mode = GPIO_MODE_OUTPUT;
	config.pull_up_en = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.intr_type = GPIO_INTR_DISABLE;
	if (gpio_config(&config) != ESP_OK) {
		return false;
	}
	return writeGpio(pin, initialValue);
}

bool lexxos::platform::readGpio(GpioPin pin, bool& value) {
	if (!isValidPin(pin)) {
		return false;
	}
	value = gpio_get_level(toEspGpio(pin)) != 0;
	return true;
}

bool lexxos::platform::writeGpio(GpioPin pin, bool value) {
	if (!isValidPin(pin)) {
		return false;
	}
	return gpio_set_level(toEspGpio(pin), value ? 1 : 0) == ESP_OK;
}

bool lexxos::platform::configureGpioInterrupt(GpioPin pin, GpioInterruptEdge edge, GpioInterruptHandler handler, void* userData) {
	if (!isValidPin(pin) || handler == nullptr) {
		return false;
	}
	if (!g_isrServiceInstalled) {
		auto const result = gpio_install_isr_service(0);
		if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
			return false;
		}
		g_isrServiceInstalled = true;
	}
	g_interrupts[static_cast<std::size_t>(pin.number)] =
	    lexxos::platform::espidf::detail::GpioInterruptSlot{handler, userData};
	gpio_set_intr_type(toEspGpio(pin), toInterruptType(edge));
	return gpio_isr_handler_add(
	           toEspGpio(pin),
	           gpioInterruptTrampoline,
	           reinterpret_cast<void*>(static_cast<std::intptr_t>(pin.number)))
	    == ESP_OK;
}

void lexxos::platform::disableGpioInterrupt(GpioPin pin) {
	if (!isValidPin(pin)) {
		return;
	}
	gpio_isr_handler_remove(toEspGpio(pin));
	gpio_set_intr_type(toEspGpio(pin), GPIO_INTR_DISABLE);
	g_interrupts[static_cast<std::size_t>(pin.number)] =
	    lexxos::platform::espidf::detail::GpioInterruptSlot{};
}
