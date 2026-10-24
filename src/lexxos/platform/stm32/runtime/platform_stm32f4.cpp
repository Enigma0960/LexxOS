#include <lexxos/platform/platform.h>

#include <cstdint>

#include <lexxos/platform/stm32/detail/stm32_console.h>
#include <lexxos/platform/stm32/detail/stm32_gpio.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

namespace {

constexpr std::uint32_t scbAircr = 0xE000ED0CU;
constexpr std::uint32_t aircrVectKey = 0x5FAU << 16U;
constexpr std::uint32_t aircrSysResetReq = 1U << 2U;
constexpr std::uint32_t systemCoreClockHz = 168000000U;

#define REG32(address) (*reinterpret_cast<volatile std::uint32_t*>(address))

void busyWaitCycles(std::uint32_t cycles) {
	while (cycles-- > 0U) {
		__asm volatile("nop");
	}
}

[[nodiscard]] bool gpioValueForActiveLevel(bool enabled, lexxos::platform::GpioActiveLevel activeLevel) {
	return activeLevel == lexxos::platform::GpioActiveLevel::High ? enabled : !enabled;
}

[[nodiscard]] bool statusLedGpioValue(bool enabled) {
	return gpioValueForActiveLevel(enabled, lexxos::platform::activeBoardDescriptor().statusLedActiveLevel);
}

} // namespace

void lexxos::platform::initialize() {
	auto const& descriptor = activeBoardDescriptor();

	// Последовательность запуска: консоль для диагностики, затем безопасное исходное состояние LED.
	stm32::detail::initializeConsole(effectiveDebugUart(descriptor));
	configureGpioOutput(descriptor.statusLed, statusLedGpioValue(false));
	writeConsole("\r\n");
	writeConsole("LexxOS STM32F407 runtime boot\r\n");
}

lexxos::platform::BoardDescriptor const& lexxos::platform::board() {
	return activeBoardDescriptor();
}

void lexxos::platform::writeConsole(std::string_view text) {
	stm32::detail::writeConsole(text);
}

bool lexxos::platform::readConsoleChar(char& value) {
	return stm32::detail::readConsoleChar(value);
}

std::size_t lexxos::platform::readConsoleAvailable(char* buffer, std::size_t capacity) {
	return stm32::detail::readConsoleAvailable(buffer, capacity);
}

void lexxos::platform::sleepMs(std::uint32_t milliseconds) {
	if (milliseconds == 0) {
		return;
	}

#if defined(LEXXOS_KERNEL_FREERTOS)
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		vTaskDelay(pdMS_TO_TICKS(milliseconds));
		return;
	}
#endif

	for (std::uint32_t index = 0; index < milliseconds; ++index) {
		busyWaitCycles(systemCoreClockHz / 12000U);
	}
}

std::uint32_t lexxos::platform::uptimeMs() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		return static_cast<std::uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
	}
#endif
	return 0;
}

lexxos::platform::PlatformPerformanceSnapshot lexxos::platform::performanceSnapshot() {
	return {
	    .systemClock = {.available = true, .hz = systemCoreClockHz},
	};
}

void lexxos::platform::setStatusLed(bool enabled) {
	writeGpio(activeBoardDescriptor().statusLed, statusLedGpioValue(enabled));
}

bool lexxos::platform::configureGpioInput(GpioPin pin, GpioPull pull) {
	return stm32::detail::configureGpioInput(pin, pull);
}

bool lexxos::platform::configureGpioOutput(GpioPin pin, bool initialValue) {
	return stm32::detail::configureGpioOutput(pin, initialValue);
}

bool lexxos::platform::readGpio(GpioPin pin, bool& value) {
	return stm32::detail::readGpio(pin, value);
}

bool lexxos::platform::writeGpio(GpioPin pin, bool value) {
	return stm32::detail::writeGpio(pin, value);
}

bool lexxos::platform::configureGpioInterrupt(GpioPin pin, GpioInterruptEdge, GpioInterruptHandler handler, void*) {
	return pin.isValid() && handler != nullptr && false;
}

void lexxos::platform::disableGpioInterrupt(GpioPin) {
}

void lexxos::platform::reboot() {
	REG32(scbAircr) = aircrVectKey | aircrSysResetReq;
	while (true) {
		__asm volatile("wfi");
	}
}

bool lexxos::platform::gameBoyRomFlashRegion(GameBoyRomFlashRegion& output) {
	output = {};
	return false;
}

bool lexxos::platform::eraseAndProgramGameBoyRomFlash(std::uint32_t, void const*, std::size_t) {
	return false;
}
