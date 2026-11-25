#include "board.h"

#include <driver/gpio.h>
#include <sdkconfig.h>

#include <lexxos/platform/platform_services.h>

namespace {

constexpr int ledGpio = CONFIG_BLINK_LED_GPIO;
static_assert(GPIO_IS_VALID_OUTPUT_GPIO(ledGpio), "LED requires an output-capable GPIO");
static_assert(ledGpio < 6 || ledGpio > 11, "GPIO6..11 are reserved for ESP-WROOM-32 flash");
static_assert(ledGpio != 1 && ledGpio != 3, "GPIO1/3 are reserved for the UART0 console");
#if CONFIG_BLINK_LED_ACTIVE_LOW
constexpr auto activeLevel = lexxos::platform::GpioActiveLevel::Low;
#else
constexpr auto activeLevel = lexxos::platform::GpioActiveLevel::High;
#endif

} // namespace

namespace lexxos::ce::blink {

using lexxos::platform::LegacyBoardDescriptor;
using lexxos::platform::ProcessorFamily;

LegacyBoardDescriptor const& boardDescriptor() {
	static LegacyBoardDescriptor const descriptor{
	    .boardId = "esp_wroom_32",
	    .boardName = "ESP-WROOM-32 / CE blink",
	    .vendor = "Espressif",
	    .processorFamily = ProcessorFamily::Esp32,
	    .processorName = "ESP32",
	    .statusLed = {ledGpio},
	    .statusLedActiveLevel = activeLevel,
	    .userGpio = {
	                  .pins = {lexxos::platform::BoardUserGpioPin::digitalOutput(
	            {ledGpio}, "led", activeLevel == lexxos::platform::GpioActiveLevel::Low)},
	                  .count = 1,
	                  },
	    .debugUart = {.index = 0, .tx = {1}, .rx = {3}, .baudRate = 115200},
	};

	return descriptor;
}

lexxos::platform::PlatformServices const& platformServices() {
	static auto const services = [] {
		auto services = lexxos::platform::services();
		services.board.current = boardDescriptor;
		return services;
	}();

	return services;
}

} // namespace lexxos::ce::blink
