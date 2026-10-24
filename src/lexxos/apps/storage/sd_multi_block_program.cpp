#include <lexxos/apps/storage/sd_multi_block_program.h>

#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/sd_settings_support.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec arguments[] = {
	    {
         .name = "state",
         .valueName = "on|off",
         .help = "enable or disable CMD18 multi-block reads",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor SdMultiBlockProgram::descriptor() {
	return {
	    .name = "sd-multiblock",
	    .help = "show or set SD CMD18 multi-block read usage",
	    .description = "Shows CMD18 multi-block read state. The default SPI baseline enables CMD18 for readBlocks(count > 1).",
	    .usage = "sd-multiblock [on|off]",
	    .arguments = argumentList(arguments),
	    .handler = run,
	};
}

int SdMultiBlockProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& parsedArguments) {
	if (!ensureStorageReady(context)) {
		return 1;
	}

	std::string_view const requestedText = parsedArguments.positional("state");
	if (requestedText.empty()) {
		context.write("sd-multiblock: ");
		context.writeLine(lexxos::devices::sd_card::multiBlockReadEnabled() ? "on" : "off");
		return 0;
	}

	bool enabled = false;
	if (!detail::parseOnOff(requestedText, enabled)) {
		context.writeLine("usage: sd-multiblock [on|off]");
		return 1;
	}

	lexxos::devices::sd_card::setMultiBlockRead(enabled);
	context.write("sd-multiblock: ");
	context.writeLine(lexxos::devices::sd_card::multiBlockReadEnabled() ? "on" : "off");
	return 0;
}

} // namespace lexxos::apps::storage_programs
