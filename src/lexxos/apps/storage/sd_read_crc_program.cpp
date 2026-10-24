#include <lexxos/apps/storage/sd_read_crc_program.h>

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
         .help = "enable or disable CRC16 validation for SD read blocks",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor SdReadCrcProgram::descriptor() {
	return {
	    .name = "sd-read-crc",
	    .help = "show or set SD read CRC validation",
	    .description = "Shows SD read CRC validation. In the stable build CRC validation stays off because the current SD cards/module proved more stable without this diagnostic path.",
	    .usage = "sd-read-crc [on|off]",
	    .arguments = argumentList(arguments),
	    .handler = run,
	};
}

int SdReadCrcProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& parsedArguments) {
	if (!ensureStorageReady(context)) {
		return 1;
	}

	std::string_view const requestedText = parsedArguments.positional("state");
	if (requestedText.empty()) {
		context.write("sd-read-crc: ");
		context.writeLine(lexxos::devices::sd_card::readCrcValidation() ? "on" : "off");
		return 0;
	}

	bool enabled = false;
	if (!detail::parseOnOff(requestedText, enabled)) {
		context.writeLine("usage: sd-read-crc [on|off]");
		return 1;
	}

#if !defined(LEXXOS_ENABLE_SD_SPEED_EXPERIMENTS)
	if (enabled) {
		context.writeLine("sd-read-crc: disabled in stable build");
		return 1;
	}
#endif

	lexxos::devices::sd_card::setReadCrcValidation(enabled);
	context.write("sd-read-crc: ");
	context.writeLine(lexxos::devices::sd_card::readCrcValidation() ? "on" : "off");
	return 0;
}

} // namespace lexxos::apps::storage_programs
