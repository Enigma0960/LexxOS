#include "board.h"

#include <sdkconfig.h>

namespace analog_pwm {

namespace {

	constexpr lexxos::platform::AnalogInputConfig inputs[]{
	    {.name = "knob", .pin = {CONFIG_DEMO_ADC_GPIO}, .resolutionBits = 12, .attenuation = lexxos::platform::AnalogAttenuation::Db12},
	};
	constexpr lexxos::platform::PwmOutputConfig outputs[]{
	    {.name = "led", .pin = {CONFIG_DEMO_PWM_GPIO}, .channel = 0, .timer = 0, .frequencyHz = 1000, .resolutionBits = 10,
#if CONFIG_DEMO_PWM_ACTIVE_LOW
	     .activeLevel = lexxos::platform::GpioActiveLevel::Low,
#else
	        .activeLevel = lexxos::platform::GpioActiveLevel::High,
#endif
	     .initialDutyPermille = 0},
	};
	lexxos::platform::LegacyBoardDescriptor const& board() {
		static constexpr lexxos::platform::LegacyBoardDescriptor descriptor{
		    .boardId = "esp_wroom_32",
		    .boardName = "ESP-WROOM-32 / Analog input and PWM",
		    .processorFamily = lexxos::platform::ProcessorFamily::Esp32,
		    .debugUart = {.index = 0, .tx = {1}, .rx = {3}, .baudRate = 115200},
		    .analogInputs = inputs,
		    .pwmOutputs = outputs,
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

} // namespace analog_pwm
