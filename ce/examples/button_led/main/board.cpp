#include "board.h"

#include <driver/gpio.h>
#include <sdkconfig.h>

namespace button_led {

namespace {

	using Pin = lexxos::platform::BoardUserGpioPin;
	constexpr int buttonPin = CONFIG_DEMO_BUTTON_GPIO;
	constexpr int ledPin = CONFIG_DEMO_LED_GPIO;

	constexpr bool available(int pin) {
		return pin != 1 && pin != 3 && (pin < 6 || pin > 11);
	}

	static_assert(available(buttonPin) && GPIO_IS_VALID_GPIO(buttonPin) && buttonPin < 34,
	    "Button requires a free pin with internal pull-up");
	static_assert(available(ledPin) && GPIO_IS_VALID_OUTPUT_GPIO(ledPin), "LED requires a free output");
	static_assert(buttonPin != ledPin, "Button and LED must use different pins");

	lexxos::platform::LegacyBoardDescriptor const& board() {
		static constexpr lexxos::platform::LegacyBoardDescriptor descriptor{
		    .boardId = "esp_wroom_32",
		    .boardName = "ESP-WROOM-32 / Button LED",
		    .processorFamily = lexxos::platform::ProcessorFamily::Esp32,
		    .statusLed = {ledPin},
		    .userGpio = {.pins = {Pin::digitalInput({buttonPin}, "button", lexxos::platform::GpioPull::Up),
		                     Pin::digitalOutput({ledPin}, "led", false)},
		                  .count = 2},
		    .debugUart = {.index = 0, .tx = {1}, .rx = {3}, .baudRate = 115200},
		};
		return descriptor;
	}

} // namespace

lexxos::platform::PlatformServices const& platform() {
	static auto const services = [] {
		auto value = lexxos::platform::services();
		value.board.current = board;
		return value;
	}();
	return services;
}

} // namespace button_led
