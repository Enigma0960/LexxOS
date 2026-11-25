#include <lexxos/platform/rockchip/rk3506/i2c2_io.h>

#include <lexxos/platform/platform.h>

// Callbacks preserve the existing non-yielding transfer/configuration path.
extern "C" std::uint64_t rk3506_timer_counter();
extern "C" std::uint32_t rk3506_timer_counter_frequency();

namespace {

constexpr std::uintptr_t gpio0 = 0xFF940000U;
constexpr std::uintptr_t gpio0Ioc = 0xFF950000U;
constexpr std::uintptr_t pmuCru = 0xFF9B0000U;
constexpr std::uintptr_t cru = 0xFF9A0000U;
constexpr std::uintptr_t rmio = 0xFF910000U;
constexpr std::uintptr_t i2c2 = 0xFF060000U;

void write32(std::uintptr_t address, std::uint32_t value) {
	*reinterpret_cast<std::uint32_t volatile*>(address) = value;
	asm volatile("dsb sy" ::: "memory");
}

std::uint32_t read32(std::uintptr_t address) {
	return *reinterpret_cast<std::uint32_t volatile*>(address);
}

std::uint64_t counter(void*) {
	return rk3506_timer_counter();
}

std::uint32_t frequency(void*) {
	return rk3506_timer_counter_frequency();
}

bool validPins(lexxos::platform::I2cPins const& pins) {
	return pins.index == 2 && pins.sda.number == 11 && pins.scl.number == 10 && (pins.baudRate == 10000U || pins.baudRate == 100000U);
}

void releaseLine(void*, lexxos::platform::GpioPin pin, bool release) {
	auto const bit = 1U << static_cast<unsigned>(pin.number);
	// Open drain: input releases the line; output drives the zero in DR.
	write32(gpio0 + 0x08U, (bit << 16U) | (release ? 0U : bit));
}

bool lineHigh(void*, lexxos::platform::GpioPin pin) {
	return (read32(gpio0 + 0x70U) & (1U << static_cast<unsigned>(pin.number))) != 0;
}

bool configureSoftwarePins(lexxos::platform::I2cPins const& pins) {
	if (!validPins(pins)) {
		return false;
	}

	write32(pmuCru + 0x800U, (1U << 23U) | (1U << 24U));
	releaseLine(nullptr, pins.sda, true);
	releaseLine(nullptr, pins.scl, true);
	write32(gpio0, ((1U << 11U) | (1U << 10U)) << 16U);
	write32(gpio0Ioc + 0x08U, 0xFF000000U);
	return true;
}

bool configureSoftware(void*, lexxos::platform::I2cPins const& pins) {
	if (!configureSoftwarePins(pins)) {
		return false;
	}

	lexxos::platform::writeConsole("picocalc i2c: software GPIO0_B3/B2, <=100 kHz\n");
	return true;
}

std::uint32_t readRegister(void*, std::uint32_t offset) {
	return read32(i2c2 + offset);
}

void writeRegister(void*, std::uint32_t offset, std::uint32_t value) {
	write32(i2c2 + offset, value);
}

bool configureHardwarePins(lexxos::platform::I2cPins const& pins) {
	if (!validPins(pins)) {
		return false;
	}

	// RK3506 TRM v1.2 Part1: CLKSEL_CON33 [5:4]=0 selects OSC24M,
	// [3:0]=0 divides by 1. Do not use older vendor U-Boot's PLL enum here.
	write32(cru + 0x800U, 1U << 17U); // OSC24M gate, CON0 bit 1
	write32(cru + 0x830U, (1U << 18U) | (1U << 19U)); // I2C2 PCLK/CLK
	write32(cru + 0x384U, 0x003F0000U);
	if ((read32(cru + 0x384U) & 0x3FU) != 0) {
		return false;
	}

	// GIC SPI42 = INTID74. Poll peripheral event flags without delivering IRQs.
	// Mask only I2C2; do not change the timer, SPI or DMA interrupt state.
	write32(0xFF581188U, 1U << 10U); // GICD_ICENABLER2
	// Reset only I2C2; SPI0/SPI1 share this register and must keep running.
	write32(cru + 0xA30U, 0x000C000CU);

	auto const started = counter(nullptr);
	auto const ticks = (static_cast<std::uint64_t>(frequency(nullptr)) + 999999U) / 1000000U;

	while (counter(nullptr) - started < ticks) {
	}

	write32(cru + 0xA30U, 0x000C0000U);
	writeRegister(nullptr, 0x18U, 0); // no I2C interrupt delivery
	writeRegister(nullptr, 0x00U, 0);
	// RM_IO10/11 select I2C2 SCL/SDA, GPIO0_B2/B3 select RMIO function 7.
	write32(pmuCru + 0x800U, (1U << 23U) | (1U << 24U));
	write32(rmio + 0xA8U, 0x007F0013U);
	write32(rmio + 0xACU, 0x007F0014U);
	write32(gpio0Ioc + 0x08U, 0xFF007700U);

	bool const ready = (read32(gpio0Ioc + 0x08U) & 0xFF00U) == 0x7700U;
	return ready;
}

bool configureHardware(void*, lexxos::platform::I2cPins const& pins) {
	if (!configureHardwarePins(pins)) {
		return false;
	}

	lexxos::platform::writeConsole(pins.baudRate == 10000U
	        ? "picocalc i2c: hardware I2C2, 10 kHz, OSC24M\n"
	        : "picocalc i2c: hardware I2C2, 100 kHz, OSC24M\n");

	return true;
}

bool recoverHardware(void*, lexxos::platform::I2cPins const& pins) {
	// The caller owns the I2C transaction. Never reset the southbridge or SPI.
	writeRegister(nullptr, 0x18U, 0);
	writeRegister(nullptr, 0x00U, 0);
	lexxos::devices::SoftwareI2cBus busClear{
	    {nullptr,
	     [](void*, lexxos::platform::I2cPins const& p) { return configureSoftwarePins(p); },
	     releaseLine, lineHigh, counter, frequency}
    };

	bool const released = busClear.initialize(pins) && busClear.clearBus();
	// Restore pinmux/controller even when the physical bus remains stuck.
	bool const configured = configureHardwarePins(pins);
	return released && configured;
}

} // namespace

namespace lexxos::platform::rk3506 {

devices::Rk3506I2cIo hardwareI2c2Io() {
	return {nullptr, configureHardware, readRegister, writeRegister, counter, frequency, recoverHardware};
}

devices::SoftwareI2cIo softwareI2c2Io() {
	return {nullptr, configureSoftware, releaseLine, lineHigh, counter, frequency};
}

} // namespace lexxos::platform::rk3506
