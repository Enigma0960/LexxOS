#include <lexxos/platform/rockchip/rk3506/gpio_output.h>

#include <cstdint>

#if defined(LEXXOS_RK3506_GPIO_TEST)
extern "C" void rk3506_gpio_test_write(std::uintptr_t, std::uint32_t);
#endif

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::uintptr_t gpio0 = 0xFF940000U;
	constexpr std::uintptr_t gpio0Ioc = 0xFF950000U;
	constexpr std::uintptr_t pmuCru = 0xFF9B0000U;

	void write32(std::uintptr_t address, std::uint32_t value) {
#if defined(LEXXOS_RK3506_GPIO_TEST)
		rk3506_gpio_test_write(address, value);
#else
		*reinterpret_cast<std::uint32_t volatile*>(address) = value;
#endif
	}

	bool supported(unsigned pin) {
		// Preserve the previously supported mux/register subset. Other GPIO
		// routes need a separate audit, not just a wider product permission.
		return pin >= 2U && pin <= 4U;
	}

} // namespace

bool configureGpio0Output(unsigned pin, bool initialValue) {
	if (!supported(pin)) {
		return false;
	}

	write32(pmuCru + 0x800U, (1U << 23U) | (1U << 24U));
	auto const muxShift = (pin % 4U) * 4U;
	write32(gpio0Ioc + (pin < 4U ? 0U : 4U), 0xFU << (muxShift + 16U));
	auto const bit = 1U << pin;
	write32(gpio0, (bit << 16U) | (initialValue ? bit : 0U));
	write32(gpio0 + 0x08U, (bit << 16U) | bit);
	return true;
}

bool writeGpio0Output(unsigned pin, bool value) {
	if (!supported(pin)) {
		return false;
	}

	auto const bit = 1U << pin;
	write32(gpio0, (bit << 16U) | (value ? bit : 0U));
	return true;
}

} // namespace lexxos::platform::rk3506
