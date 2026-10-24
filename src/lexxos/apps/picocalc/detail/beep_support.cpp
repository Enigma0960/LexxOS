#include <lexxos/apps/picocalc/detail/beep_support.h>

#include <lexxos/apps/common/core_programs_support.h>

#if defined(LEXXOS_PLATFORM_PICO)
	#include <hardware/clocks.h>
	#include <hardware/gpio.h>
	#include <hardware/pwm.h>
#endif

namespace lexxos::apps::picocalc_programs::detail {
namespace {

#if defined(LEXXOS_PLATFORM_PICO)
	int runBeepOnPico(
	    lexxos::os::AppContext& context,
	    lexxos::platform::GpioPin pin,
	    std::uint32_t frequencyHz,
	    std::uint32_t durationMs) {
		auto const gpio = static_cast<uint>(pin.number);
		gpio_set_function(gpio, GPIO_FUNC_PWM);

		auto const slice = pwm_gpio_to_slice_num(gpio);
		constexpr std::uint16_t wrap = 1000;
		float divider =
		    static_cast<float>(clock_get_hz(clk_sys)) / (static_cast<float>(frequencyHz) * static_cast<float>(wrap + 1));
		if (divider < 1.0F) {
			divider = 1.0F;
		} else if (divider > 255.0F) {
			divider = 255.0F;
		}

		auto pwmConfig = pwm_get_default_config();
		pwm_config_set_clkdiv(&pwmConfig, divider);
		pwm_config_set_wrap(&pwmConfig, wrap);
		pwm_init(slice, &pwmConfig, false);
		pwm_set_gpio_level(gpio, wrap / 2);
		pwm_set_enabled(slice, true);
		sleepMs(context, durationMs);
		pwm_set_gpio_level(gpio, 0);
		pwm_set_enabled(slice, false);
		gpio_set_function(gpio, GPIO_FUNC_SIO);
		gpio_set_dir(gpio, GPIO_OUT);
		gpio_put(gpio, 0);
		return 0;
	}
#endif

} // namespace

int runBeepSequenceOnPico(
    lexxos::os::AppContext& context,
    lexxos::platform::AudioBeepPins const& config,
    std::uint32_t frequencyHz,
    std::uint32_t durationMs) {
#if defined(LEXXOS_PLATFORM_PICO)
	bool played = false;
	if (config.hasLeft()) {
		context.write("beep: left pin=");
		writePinValue(context, config.left);
		context.writeLine("");
		(void) runBeepOnPico(context, config.left, frequencyHz, durationMs);
		played = true;
	}
	if (config.hasRight()) {
		if (played) {
			sleepMs(context, 80);
		}
		context.write("beep: right pin=");
		writePinValue(context, config.right);
		context.writeLine("");
		(void) runBeepOnPico(context, config.right, frequencyHz, durationMs);
		played = true;
	}
	return played ? 0 : 1;
#else
	(void) context;
	(void) config;
	(void) frequencyHz;
	(void) durationMs;
	return 1;
#endif
}

} // namespace lexxos::apps::picocalc_programs::detail
