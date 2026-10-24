#include <lexxos/apps/rtc/rtc_set_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/rtc/detail/rtc_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::rtc_programs {

using lexxos::devices::rtc::DateTime;
using lexxos::program::ProgramDescriptor;

ProgramDescriptor RtcSetProgram::descriptor() {
	return {
	    .name = "rtc-set",
	    .help = "set RTC date and time",
	    .description = "Writes date/time to the selected RTC backend. Supported range is 2000-01-01 through 2199-12-31; PicoCalc JCS RTC supports 2000-2099, software RTC supports the 32-bit Unix time range.",
	    .usage = "rtc-set [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>] <YYYY-MM-DD> <HH:MM:SS>",
	    .arguments = lexxos::apps::argumentList(detail::rtcSetArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int RtcSetProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	DateTime value{};
	if (!detail::parseDateTime(arguments.positional("date"), arguments.positional("time"), value)) {
		context.writeErrorLine("rtc-set: invalid date/time, expected YYYY-MM-DD HH:MM:SS or YYYY-MM-DDTHH:MM:SS");
		return 2;
	}

	detail::RtcBackend backend{};
	if (!detail::openRtcBackend(context, "rtc-set", arguments, backend)) {
		return 1;
	}

	if (!detail::writeDateTime(backend, value, uptimeMs(context))) {
		context.writeErrorLine("rtc-set: failed to write RTC date/time");
		return 1;
	}

	context.write("source: ");
	context.writeLine(detail::backendName(backend.kind));
	context.write("rtc set: ");
	detail::writeDateTime(context, value);
	context.writeLine("");
	return 0;
}

} // namespace lexxos::apps::rtc_programs
