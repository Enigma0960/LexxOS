#include <lexxos/apps/storage/sd_baud_program.h>

#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/sd_settings_support.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec arguments[] = {
	    {
         .name = "hz",
         .valueName = "safe|default|performance|hz",
         .help = "SPI baud rate/profile to set: safe, default, performance or Hz; without argument prints active rate",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor SdBaudProgram::descriptor() {
	return {
	    .name = "sd-baud",
	    .help = "show or set SD SPI baud rate",
	    .description = "Shows the current SD SPI baud rate or sets a new transfer rate/profile after initialization.",
	    .usage = "sd-baud [safe|default|performance|hz]",
	    .arguments = argumentList(arguments),
	    .handler = run,
	};
}

int SdBaudProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& parsedArguments) {
	if (!ensureStorageReady(context)) {
		return 1;
	}

	std::string_view const requestedText = parsedArguments.positional("hz");
	if (requestedText.empty()) {
		context.write("sd-baud: active ");
		writeUnsigned(context, lexxos::devices::sd_card::activeBaudRate());
		context.writeLine(" Hz");
		return 0;
	}

	std::uint32_t requested = 0;
	if (requestedText == "safe") {
		requested = detail::sdSafeBaudRate();
	} else if (requestedText == "default" || requestedText == "stable") {
		requested = detail::sdStableBaudRate();
	} else if (requestedText == "performance" || requestedText == "perf") {
		requested = detail::sdPerformanceBaudRate();
	} else if (!parseUnsigned32(requestedText, requested) || requested < detail::sdSpeedMinimumBaudRate()) {
		context.writeLine("usage: sd-baud [safe|default|performance|hz]");
		context.write("sd-baud: hz must be >= ");
		writeUnsigned(context, detail::sdSpeedMinimumBaudRate());
		context.writeLine("");
		return 1;
	}

	if (requested > detail::sdMaximumBaudRate()) {
		context.write("sd-baud: maximum configured SD baud is ");
		writeUnsigned(context, detail::sdMaximumBaudRate());
		context.writeLine(" Hz");
		return 1;
	}

	if (!lexxos::devices::sd_card::setTransferBaudRate(requested)) {
		context.writeLine("sd-baud: set failed");
		return 1;
	}

	context.write("sd-baud: active ");
	writeUnsigned(context, lexxos::devices::sd_card::activeBaudRate());
	context.writeLine(" Hz");
	return 0;
}

} // namespace lexxos::apps::storage_programs
