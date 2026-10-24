#include <lexxos/apps/storage/sd_debug_program.h>

#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/storage/detail/sd_diagnostics_support.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs {

using lexxos::program::ProgramDescriptor;

namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramArgumentValueType;

	constexpr ProgramArgumentSpec sdDebugArguments[] = {
	    {
         .name = "action",
         .valueName = "status|on|off|clear",
         .help = "optional diagnostic action; default: status",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

ProgramDescriptor SdDebugProgram::descriptor() {
	return {
	    .name = "sd-debug",
	    .help = "show or control SD driver diagnostics",
	    .description = "Shows the SD SPI diagnostic ring buffer and can enable, disable or clear recorded driver errors.",
	    .usage = "sd-debug [status|on|off|clear]",
	    .arguments = argumentList(sdDebugArguments),
	    .handler = run,
	};
}

int SdDebugProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::string_view const action = arguments.positional("action");
	if (action.empty() || action == "status" || action == "show") {
		detail::printSdDiagnostics(context);
		return 0;
	}

	if (action == "on" || action == "enable") {
		lexxos::devices::sd_card::setDiagnosticsEnabled(true);
		context.writeLine("sd-debug: diagnostics enabled");
		detail::printSdDiagnostics(context);
		return 0;
	}

	if (action == "off" || action == "disable") {
		lexxos::devices::sd_card::setDiagnosticsEnabled(false);
		context.writeLine("sd-debug: diagnostics disabled");
		detail::printSdDiagnostics(context);
		return 0;
	}

	if (action == "clear" || action == "reset") {
		lexxos::devices::sd_card::clearDiagnostics();
		context.writeLine("sd-debug: diagnostics cleared");
		return 0;
	}

	context.writeLine("usage: sd-debug [status|on|off|clear]");
	return 1;
}

} // namespace lexxos::apps::storage_programs
