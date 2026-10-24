#include <lexxos/apps/utility/led_program.h>

#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::apps::utility_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec ledArguments[] = {
	    {
         .name = "state",
         .valueName = "on|off|toggle",
         .help = "status LED state",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = true,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor LedProgram::descriptor() {
	return {
	    .name = "led",
	    .help = "set status LED state",
	    .usage = "led <on|off|toggle>",
	    .arguments = argumentList(ledArguments),
	    .handler = run,
	};
}

int LedProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const& gpio = context.platform().gpio;
	if (gpio.setStatusLed == nullptr) {
		context.writeLine("status LED unavailable");
		return 1;
	}

	static bool ledEnabled = false;
	std::string_view const state = arguments.positional("state");
	if (state == "on") {
		ledEnabled = true;
		gpio.setStatusLed(ledEnabled);
		return 0;
	}
	if (state == "off") {
		ledEnabled = false;
		gpio.setStatusLed(ledEnabled);
		return 0;
	}
	if (state == "toggle") {
		ledEnabled = !ledEnabled;
		gpio.setStatusLed(ledEnabled);
		return 0;
	}
	context.writeLine("usage: led <on|off|toggle>");
	return 1;
}

} // namespace lexxos::apps::utility_programs
