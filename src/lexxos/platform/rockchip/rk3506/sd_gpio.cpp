#include <lexxos/platform/rockchip/rk3506/sd_gpio.h>

#include <cstddef>

#if defined(LEXXOS_RK3506_SD_GPIO_TEST)
extern "C" std::uint32_t sd_gpio_test_read(std::uintptr_t);
extern "C" void sd_gpio_test_write(std::uintptr_t, std::uint32_t);
#endif

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::uintptr_t kCru = 0xFF9A0000U;
	constexpr std::uintptr_t kIoc1 = 0xFF660000U;
	constexpr std::uintptr_t kGpio1 = 0xFF870000U;
	// RK3506 GPIO v2 register layout, high half = port pins 16..31.
	constexpr std::uintptr_t kDrHigh = 0x04U;
	constexpr std::uintptr_t kDdrHigh = 0x0CU;
	constexpr std::uintptr_t kExtPort = 0x70U;
	constexpr std::uint32_t kMosi = 1U << (19U - 16U);
	constexpr std::uint32_t kClock = 1U << (25U - 16U);
	constexpr std::uint32_t kCs = 1U << (26U - 16U);
	constexpr std::uint32_t kMisoInput = 1U << 27U;
	constexpr std::uint32_t kDetectInput = 1U << 10U;
	constexpr std::uint32_t kOutputs = kMosi | kClock | kCs;

	std::uint32_t read32(std::uintptr_t addr) {
#if defined(LEXXOS_RK3506_SD_GPIO_TEST)
		return sd_gpio_test_read(addr);
#else
		return *reinterpret_cast<std::uint32_t const volatile*>(addr);
#endif
	}

	void write32(std::uintptr_t addr, std::uint32_t value) {
#if defined(LEXXOS_RK3506_SD_GPIO_TEST)
		sd_gpio_test_write(addr, value);
#else
		*reinterpret_cast<std::uint32_t volatile*>(addr) = value;
#endif
	}

	void setOutputs(std::uint32_t mask, std::uint32_t values) {
		// GPIO v2 write-mask: [31:16] enable, [15:0] values.
		write32(kGpio1 + kDrHigh, (mask << 16U) | (values & mask));
	}

	void delayEdge() {
		// Conservative software SPI; no fast-path or DMA until board validation.
		for (unsigned i = 0; i < 48U; ++i) {
			asm volatile("" ::: "memory");
		}
	}

} // namespace

void sdGpioConfigure() {
	// Enable only PCLK_GPIO1: CRU_CLKGATE_CON3, bit 8, hiword write mask.
	// This does not alter the boot SDMMC clocks, pad mux or controller.
	write32(kCru + 0x80CU, 1U << (16U + 8U));
	// Drive inactive CS/MOSI before selecting the pins as GPIO.
	setOutputs(kOutputs, kCs | kMosi);
	write32(kGpio1 + kDdrHigh,
	    ((kOutputs | (1U << (27U - 16U))) << 16U) | kOutputs);
	// C3 / D1 / D2 / D3: GPIO function 0 instead of RMIO function 7.
	// Hiword masks change only the four dedicated PicoCalc SD pins.
	write32(kIoc1 + 0x30U, 0xF0000000U); // C3: bits 15:12
	write32(kIoc1 + 0x38U, 0xFFF00000U); // D1..D3: bits 15:4
}

void sdGpioChipSelect(bool high) {
	setOutputs(kCs, high ? kCs : 0U);
	delayEdge();
}

std::uint8_t sdGpioTransfer(std::uint8_t output) {
	std::uint8_t input = 0;
	for (unsigned bit = 0; bit != 8; ++bit) {
		setOutputs(kMosi | kClock, (output & 0x80U) ? kMosi : 0U);
		delayEdge();
		setOutputs(kClock, kClock);
		delayEdge();
		input = static_cast<std::uint8_t>((input << 1U) | ((read32(kGpio1 + kExtPort) & kMisoInput) ? 1U : 0U));
		setOutputs(kClock, 0U);
		output = static_cast<std::uint8_t>(output << 1U);
		delayEdge();
	}

	return input;
}

void sdGpioEnableClock() {
	write32(kCru + 0x80CU, 1U << (16U + 8U));
}

bool sdGpioCardInserted() {
	return (read32(kGpio1 + kExtPort) & kDetectInput) == 0U;
}

SdGpioRegisters sdGpioRegisters(bool configured) {
	auto const gate = read32(kCru + 0x80CU);
	bool const readable = configured || (gate & (1U << 8U)) == 0U;
	return {gate, read32(kCru + 0x830U), read32(kIoc1 + 0x30U), read32(kIoc1 + 0x38U),
	    readable, readable && !sdGpioCardInserted()};
}

} // namespace lexxos::platform::rk3506
