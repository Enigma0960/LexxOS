#include <lexxos/apps/storage/sd_multi_write_program.h>

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
         .help = "enable or disable experimental CMD25 multi-block writes",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor SdMultiWriteProgram::descriptor() {
	return {
	    .name = "sd-multiwrite",
	    .help = "show or set experimental SD CMD25 multi-block writes",
	    .description = "Shows or changes CMD25 multi-block write usage. Keep this off for the stable FAT path; use it only for isolated diagnostics.",
	    .usage = "sd-multiwrite [on|off]",
	    .arguments = argumentList(arguments),
	    .handler = run,
	};
}

int SdMultiWriteProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& parsedArguments) {
	if (!ensureStorageReady(context)) {
		return 1;
	}

	std::string_view const requestedText = parsedArguments.positional("state");
	if (requestedText.empty()) {
		context.write("sd-multiwrite: ");
		context.writeLine(lexxos::devices::sd_card::multiBlockWriteEnabled() ? "on" : "off");
		return 0;
	}

	bool enabled = false;
	if (!detail::parseOnOff(requestedText, enabled)) {
		context.writeLine("usage: sd-multiwrite [on|off]");
		return 1;
	}

	lexxos::devices::sd_card::setMultiBlockWrite(enabled);
	context.write("sd-multiwrite: ");
	context.writeLine(lexxos::devices::sd_card::multiBlockWriteEnabled() ? "on" : "off");
	if (lexxos::devices::sd_card::multiBlockWriteEnabled()) {
		context.writeLine("sd-multiwrite: warning: CMD25 is experimental on this SPI backend");
	}
	return 0;
}

} // namespace lexxos::apps::storage_programs
