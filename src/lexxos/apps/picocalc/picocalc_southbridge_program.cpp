#include <commands/command_selection.h>

#include <lexxos/apps/picocalc/picocalc_southbridge_program.h>

#include <cstddef>

#include <lexxos/apps/picocalc/detail/picocalc_southbridge_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::i2c_programs {

using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentList;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;

namespace {

	template <std::size_t Size>
	constexpr ProgramArgumentList argumentList(ProgramArgumentSpec const (&arguments)[Size]) {
		return ProgramArgumentList{
		    .data = arguments,
		    .size = Size,
		};
	}

	constexpr ProgramArgumentSpec picoCalcSouthbridgeArguments[] = {
	    {
         .name = "bus",
         .shortName = 'b',
         .valueName = "name",
         .help = "I2C bus name",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "debug",
	     },
	    {
         .name = "address",
         .shortName = 'a',
         .valueName = "addr",
         .help = "southbridge 7-bit I2C address",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0x1F",
	     },
	    {
         .name = "poll",
         .shortName = 'p',
         .valueName = "count",
         .help = "deprecated keyboard FIFO reads; skipped while picocalc-keyboard owns input",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0",
	     },
	    {
         .name = "delay",
         .shortName = 'd',
         .valueName = "ms",
         .help = "delay between deprecated keyboard FIFO reads",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "50",
	     },
	    {
         .name = "lcd",
         .valueName = "0..255",
         .help = "set LCD backlight brightness",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "kbd",
         .valueName = "0..255",
         .help = "set keyboard backlight brightness",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "reset",
         .valueName = "seconds",
         .help = "ask PicoCalc southbridge to reset the device after a delay",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "direct-i2c",
         .help = "diagnostic-only direct I2C access when no southbridge service is registered",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	};

} // namespace

ProgramDescriptor PicoCalcSouthbridgeProgram::descriptor() {
	return {
	    .name = "picocalc-sb",
	    .help = "diagnose PicoCalc southbridge over I2C",
	    .description = "Shows PicoCalc STM32 southbridge status through the registered service, with explicit direct-I2C diagnostics for bring-up.",
	    .usage = "picocalc-sb [--direct-i2c] [--bus <name>] [--address <addr>] [--poll <count>] [--delay <ms>] [--lcd <0..255>] [--kbd <0..255>] [--reset <seconds>]",
	    .arguments = argumentList(picoCalcSouthbridgeArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int PicoCalcSouthbridgeProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	picocalc_southbridge_detail::CommandOptions options{};
	if (!picocalc_southbridge_detail::parseCommandOptions(context, arguments, options)) {
		return 2;
	}

	if (picocalc_southbridge_detail::hasSouthbridgeService(context)) {
		return picocalc_southbridge_detail::runServiceBacked(context, arguments, options);
	}

	if (!options.directRequested) {
		picocalc_southbridge_detail::writeDirectI2cRequired(context);
		return 1;
	}

#if LEXXOS_COMMAND_TEST_OPERATIONS
	return picocalc_southbridge_detail::runDirectI2c(context, arguments, options);
#else
	context.writeErrorLine("picocalc-sb: direct I2C tests are disabled in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::i2c_programs
