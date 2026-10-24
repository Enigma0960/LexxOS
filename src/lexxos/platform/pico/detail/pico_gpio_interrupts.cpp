#include <lexxos/platform/pico/detail/pico_gpio_interrupts.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <hardware/gpio.h>

#include <lexxos/platform/pico/pico_platform_types.h>

namespace lexxos::platform::pico::detail {
namespace {

	constexpr std::size_t maxGpioInterruptRegistrations = 8;
	std::array<GpioInterruptRegistration, maxGpioInterruptRegistrations> g_gpioInterrupts{};

	uint toPicoGpioPin(lexxos::platform::GpioPin pin) {
		return static_cast<uint>(pin.number);
	}

	std::uint32_t toPicoGpioEvents(lexxos::platform::GpioInterruptEdge edge) {
		switch (edge) {
			case lexxos::platform::GpioInterruptEdge::Rising:
				return GPIO_IRQ_EDGE_RISE;
			case lexxos::platform::GpioInterruptEdge::Falling:
				return GPIO_IRQ_EDGE_FALL;
			case lexxos::platform::GpioInterruptEdge::Both:
				return GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
		}
		return GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
	}

	GpioInterruptRegistration* findGpioInterruptRegistration(lexxos::platform::GpioPin pin) {
		for (auto& registration : g_gpioInterrupts) {
			if (registration.active && registration.pin.number == pin.number) {
				return &registration;
			}
		}
		return nullptr;
	}

	GpioInterruptRegistration* allocateGpioInterruptRegistration(lexxos::platform::GpioPin pin) {
		if (auto* existing = findGpioInterruptRegistration(pin); existing != nullptr) {
			return existing;
		}

		for (auto& registration : g_gpioInterrupts) {
			if (!registration.active) {
				return &registration;
			}
		}
		return nullptr;
	}

	void gpioInterruptCallback(uint gpio, std::uint32_t) {
		auto* registration = findGpioInterruptRegistration({.number = static_cast<int>(gpio)});
		if (registration == nullptr || registration->handler == nullptr) {
			return;
		}

		bool const value = gpio_get(gpio) != 0;
		registration->handler(registration->pin, value, registration->userData);
	}

} // namespace

bool configureGpioInterrupt(lexxos::platform::GpioPin pin, lexxos::platform::GpioInterruptEdge edge,
    lexxos::platform::GpioInterruptHandler handler, void* userData) {
	if (!pin.isValid() || handler == nullptr) {
		return false;
	}

	auto* registration = allocateGpioInterruptRegistration(pin);
	if (registration == nullptr) {
		return false;
	}

	registration->active = true;
	registration->pin = pin;
	registration->handler = handler;
	registration->userData = userData;

	uint const gpio = toPicoGpioPin(pin);
	gpio_set_irq_enabled_with_callback(gpio, toPicoGpioEvents(edge), true, gpioInterruptCallback);
	return true;
}

void disableGpioInterrupt(lexxos::platform::GpioPin pin) {
	if (!pin.isValid()) {
		return;
	}

	uint const gpio = toPicoGpioPin(pin);
	gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
	if (auto* registration = findGpioInterruptRegistration(pin); registration != nullptr) {
		*registration = {};
	}
}

} // namespace lexxos::platform::pico::detail
