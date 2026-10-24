#include <lexxos/apps/rtc/rtc_get_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/rtc/detail/rtc_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::rtc_programs {

using lexxos::devices::rtc::DateTime;
using lexxos::program::ProgramDescriptor;

ProgramDescriptor RtcGetProgram::descriptor() {
	return {
	    .name = "rtc-get",
	    .help = "read RTC date and time",
	    .description = "Reads current date/time from the selected RTC backend.",
	    .usage = "rtc-get [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>]",
	    .arguments = lexxos::apps::argumentList(detail::rtcCommonArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int RtcGetProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	detail::RtcBackend backend{};
	if (!detail::openRtcBackend(context, "rtc-get", arguments, backend)) {
		return 1;
	}

	DateTime value{};
	if (!detail::readDateTime(backend, value)) {
		context.writeErrorLine("rtc-get: failed to read or decode RTC date/time");
		return 1;
	}

	context.write("source: ");
	context.writeLine(detail::backendName(backend.kind));
	context.write("rtc: ");
	detail::writeDateTime(context, value);
	context.writeLine("");
	context.write("weekday: ");
	writeUnsigned(context, value.weekday);
	context.writeLine(" (1=Monday, 7=Sunday)");

	detail::RtcBackendStatus status{};
	if (detail::readStatus(backend, status) && status.oscillatorStoppedValid && status.oscillatorStopped) {
		context.writeLine("warning: oscillator stop flag is set, time may be invalid");
	}
	if (status.voltageLowValid && status.voltageLow) {
		context.writeLine("warning: voltage low flag is set, time may be invalid");
	}
	return 0;
}

} // namespace lexxos::apps::rtc_programs
