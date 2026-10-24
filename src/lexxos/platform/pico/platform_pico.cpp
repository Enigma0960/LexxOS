#include <lexxos/platform/platform.h>

#include <hardware/clocks.h>
#include <hardware/vreg.h>
#include <hardware/watchdog.h>
#include <pico/stdlib.h>
#include <pico/time.h>

#include <lexxos/platform/pico/detail/pico_console.h>
#include <lexxos/platform/pico/detail/pico_gpio.h>

namespace {

void configureSystemClock() {
#if defined(LEXXOS_PICO_SYS_CLOCK_KHZ) && LEXXOS_PICO_SYS_CLOCK_KHZ > 0
	#if LEXXOS_PICO_SYS_CLOCK_KHZ >= 250000
	vreg_set_voltage(VREG_VOLTAGE_1_20);
	sleep_ms(10);
	#endif
	if (!set_sys_clock_khz(LEXXOS_PICO_SYS_CLOCK_KHZ, false)) {
		set_sys_clock_khz(125000, true);
	}
#endif
}

} // namespace

void lexxos::platform::initialize() {
	// Последовательность запуска: сначала clock/vreg, затем UART и только после него GPIO-индикация.
	configureSystemClock();
	pico::detail::initializeConsole();
	pico::detail::initializeStatusLed();
}

lexxos::platform::BoardDescriptor const& lexxos::platform::board() {
	return activeBoardDescriptor();
}

void lexxos::platform::sleepMs(std::uint32_t milliseconds) {
	sleep_ms(milliseconds);
}

std::uint32_t lexxos::platform::uptimeMs() {
	return static_cast<std::uint32_t>(to_ms_since_boot(get_absolute_time()));
}

lexxos::platform::PlatformPerformanceSnapshot lexxos::platform::performanceSnapshot() {
	return {
	    .systemClock = {.available = true,  .hz = clock_get_hz(clk_sys)},
	    .peripheralClock = {.available = true, .hz = clock_get_hz(clk_peri)},
	    .referenceClock = {.available = true,  .hz = clock_get_hz(clk_ref)},
	    .usbClock = {.available = true,  .hz = clock_get_hz(clk_usb)},
	    .adcClock = {.available = true,  .hz = clock_get_hz(clk_adc)},
#if defined(PICO_RP2040) && PICO_RP2040
	    .rtcClock = {.available = true,  .hz = clock_get_hz(clk_rtc)},
#endif
	};
}

void lexxos::platform::reboot() {
	watchdog_reboot(0, 0, 0);
	while (true) {
		tight_loop_contents();
	}
}
