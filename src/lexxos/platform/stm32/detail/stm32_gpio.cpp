#include <lexxos/platform/stm32/detail/stm32_gpio.h>

namespace lexxos::platform::stm32::detail {
namespace {

	constexpr std::uint32_t peripheralBase = 0x40000000U;
	constexpr std::uint32_t ahb1Base = peripheralBase + 0x00020000U;
	constexpr std::uint32_t rccBase = ahb1Base + 0x3800U;

	constexpr std::uint32_t gpioBase(char port) {
		return ahb1Base + (static_cast<std::uint32_t>(port - 'A') * 0x400U);
	}

#define REG32(address) (*reinterpret_cast<volatile std::uint32_t*>(address))

#define RCC_AHB1ENR REG32(rccBase + 0x30U)

#define GPIO_MODER(base) REG32((base) + 0x00U)
#define GPIO_OTYPER(base) REG32((base) + 0x04U)
#define GPIO_OSPEEDR(base) REG32((base) + 0x08U)
#define GPIO_PUPDR(base) REG32((base) + 0x0CU)
#define GPIO_IDR(base) REG32((base) + 0x10U)
#define GPIO_BSRR(base) REG32((base) + 0x18U)
#define GPIO_AFRL(base) REG32((base) + 0x20U)
#define GPIO_AFRH(base) REG32((base) + 0x24U)

	constexpr std::uint32_t gpioClockMask(char port) {
		if (port < 'A' || port > 'G') {
			return 0;
		}
		return 1U << static_cast<std::uint32_t>(port - 'A');
	}

	bool enableGpioClock(char port) {
		std::uint32_t const mask = gpioClockMask(port);
		if (mask == 0) {
			return false;
		}

		RCC_AHB1ENR |= mask;
		(void) RCC_AHB1ENR;
		return true;
	}

	void configurePinMode(DecodedPin pin, std::uint32_t mode) {
		GPIO_MODER(pin.base) = (GPIO_MODER(pin.base) & ~(3U << (pin.bit * 2U))) | ((mode & 3U) << (pin.bit * 2U));
	}

	void configurePinPull(DecodedPin pin, GpioPull pull) {
		std::uint32_t value = 0;
		switch (pull) {
			case GpioPull::None:
				value = 0;
				break;
			case GpioPull::Up:
				value = 1;
				break;
			case GpioPull::Down:
				value = 2;
				break;
		}
		GPIO_PUPDR(pin.base) = (GPIO_PUPDR(pin.base) & ~(3U << (pin.bit * 2U))) | (value << (pin.bit * 2U));
	}

	void configurePinAlternateFunction(DecodedPin pin, std::uint32_t alternateFunction) {
		if (pin.bit < 8U) {
			GPIO_AFRL(pin.base) = (GPIO_AFRL(pin.base) & ~(0xFU << (pin.bit * 4U))) | ((alternateFunction & 0xFU) << (pin.bit * 4U));
		} else {
			std::uint32_t const shift = (pin.bit - 8U) * 4U;
			GPIO_AFRH(pin.base) = (GPIO_AFRH(pin.base) & ~(0xFU << shift)) | ((alternateFunction & 0xFU) << shift);
		}
	}

} // namespace

bool decodePin(GpioPin pin, DecodedPin& output) {
	if (!pin.isValid()) {
		return false;
	}

	int const portIndex = pin.number / 16;
	int const bit = pin.number % 16;
	if (portIndex < 0 || portIndex > 6 || bit < 0 || bit > 15) {
		return false;
	}

	output.port = static_cast<char>('A' + portIndex);
	output.bit = static_cast<std::uint32_t>(bit);
	output.base = gpioBase(output.port);
	return true;
}

bool configureGpioInput(GpioPin pin, GpioPull pull) {
	DecodedPin decoded{};
	if (!decodePin(pin, decoded) || !enableGpioClock(decoded.port)) {
		return false;
	}

	configurePinMode(decoded, 0U);
	configurePinPull(decoded, pull);
	return true;
}

bool configureGpioOutput(GpioPin pin, bool initialValue) {
	DecodedPin decoded{};
	if (!decodePin(pin, decoded) || !enableGpioClock(decoded.port)) {
		return false;
	}

	configurePinMode(decoded, 1U);
	GPIO_OTYPER(decoded.base) &= ~(1U << decoded.bit);
	GPIO_OSPEEDR(decoded.base) |= 2U << (decoded.bit * 2U);
	return lexxos::platform::stm32::detail::writeGpio(pin, initialValue);
}

bool readGpio(GpioPin pin, bool& value) {
	DecodedPin decoded{};
	if (!decodePin(pin, decoded)) {
		return false;
	}

	value = (GPIO_IDR(decoded.base) & (1U << decoded.bit)) != 0U;
	return true;
}

bool writeGpio(GpioPin pin, bool value) {
	DecodedPin decoded{};
	if (!decodePin(pin, decoded)) {
		return false;
	}

	GPIO_BSRR(decoded.base) = value ? (1U << decoded.bit) : (1U << (decoded.bit + 16U));
	return true;
}

bool configureAlternateFunction(GpioPin pin, std::uint32_t alternateFunction, GpioPull pull, std::uint32_t speed) {
	DecodedPin decoded{};
	if (!decodePin(pin, decoded) || !enableGpioClock(decoded.port)) {
		return false;
	}

	configurePinMode(decoded, 2U);
	GPIO_OTYPER(decoded.base) &= ~(1U << decoded.bit);
	GPIO_OSPEEDR(decoded.base) |= (speed & 3U) << (decoded.bit * 2U);
	configurePinPull(decoded, pull);
	configurePinAlternateFunction(decoded, alternateFunction);
	return true;
}

} // namespace lexxos::platform::stm32::detail
