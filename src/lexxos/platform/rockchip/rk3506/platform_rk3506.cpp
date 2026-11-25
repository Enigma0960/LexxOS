/* RK3506G2 console, clock and unsupported platform-service adapters. */

#include <cstdint>
#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <semphr.h>
	#include <task.h>
#endif

#include <lexxos/kernel/scheduler.h>
#include <lexxos/platform/platform.h>

namespace {

#if defined(LEXXOS_KERNEL_FREERTOS)
extern "C" std::uint32_t volatile ulPortInterruptNesting;
SemaphoreHandle_t consoleMutex{};
#endif

extern "C" void rk3506_uart_write_bytes(char const*, std::size_t);
extern "C" void rk3506_uart_write_raw_bytes(char const*, std::size_t);
extern "C" std::size_t rk3506_uart_read_available(char*, std::size_t);
extern "C" std::uint32_t rk3506_timer_ticks();
#if defined(LEXXOS_RK3506_FREERTOS)
extern "C" bool rk3506_uart_initialize_rx_interrupts();
#endif

void writeConsoleLocked(std::string_view bytes, void (*write)(char const*, std::size_t)) {
#if defined(LEXXOS_KERNEL_FREERTOS)
	bool const lock = ulPortInterruptNesting == 0 && consoleMutex && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
	if (lock) {
		(void) xSemaphoreTake(consoleMutex, portMAX_DELAY);
	}
#endif
	write(bytes.data(), bytes.size());
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (lock) {
		(void) xSemaphoreGive(consoleMutex);
	}
#endif
}

} // namespace

namespace lexxos::platform {

void initialize() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (!consoleMutex) {
		consoleMutex = xSemaphoreCreateMutex();
	}
#endif
#if defined(LEXXOS_RK3506_FREERTOS)
	if (!rk3506_uart_initialize_rx_interrupts()) {
		writeConsole("UART RX IRQ unavailable; using polling\n");
	}
#endif
} // DDR, clock and UART0 configured by U-Boot.

void writeConsole(std::string_view text) {
	writeConsoleLocked(text, rk3506_uart_write_bytes);
}

void writeRawConsole(std::string_view bytes) {
	writeConsoleLocked(bytes, rk3506_uart_write_raw_bytes);
}

bool readConsoleChar(char& value) {
	return rk3506_uart_read_available(&value, 1) == 1;
}

std::size_t readConsoleAvailable(char* buffer, std::size_t capacity) {
	return rk3506_uart_read_available(buffer, capacity);
}

void sleepMs(std::uint32_t ms) {
	lexxos::kernel::sleepMs(ms);
}

std::uint32_t uptimeMs() {
	return rk3506_timer_ticks();
}

void setStatusLed(bool) {
}

bool configureGpioInput(GpioPin, GpioPull) {
	return false;
}

bool readGpio(GpioPin, bool&) {
	return false;
}

bool configureGpioInterrupt(GpioPin, GpioInterruptEdge, GpioInterruptHandler, void*) {
	return false;
}

void disableGpioInterrupt(GpioPin) {
}

void reboot() {
	writeConsole("RK3506: reboot is not supported yet\n");
}

PlatformPerformanceSnapshot performanceSnapshot() {
	return {};
}

bool readBatteryStatus(PlatformBatteryStatus& output) {
	output = {};
	return false;
}

bool gameBoyRomFlashRegion(GameBoyRomFlashRegion& output) {
	output = {};
	return false;
}

bool eraseAndProgramGameBoyRomFlash(std::uint32_t, void const*, std::size_t) {
	return false;
}

bool gameBoyRomPsramRegion(GameBoyRomExternalMemoryRegion& output) {
	output = {};
	return false;
}

bool readGameBoyRomPsram(std::uint32_t, void*, std::size_t) {
	return false;
}

bool writeGameBoyRomPsram(std::uint32_t, void const*, std::size_t) {
	return false;
}

bool gameBoyRomPsramRuntimeInfo(GameBoyRomPsramRuntimeInfo& output) {
	output = {};
	return false;
}

} // namespace lexxos::platform
