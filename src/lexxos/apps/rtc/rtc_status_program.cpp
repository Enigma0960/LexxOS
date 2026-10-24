#include <lexxos/apps/rtc/rtc_status_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/rtc/detail/rtc_program_support.h>
#include <lexxos/apps/rtc/detail/rtc_settings_store.h>
#include <lexxos/feature.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::rtc_programs {

using lexxos::program::ProgramDescriptor;

namespace {

	void writeSoftwareStatus(lexxos::os::AppContext& context, lexxos::devices::rtc::software::Status const& software) {
		context.write("valid: ");
		context.writeLine(software.valid ? "yes" : "no");
		if (software.valid) {
			context.write("rtc: ");
			detail::writeDateTime(context, software.now);
			context.writeLine("");
			context.write("unix: ");
			writeUnsigned(context, software.unixSeconds);
			context.writeLine("");
		}
		context.write("last-set: ");
		context.writeLine(lexxos::devices::rtc::software::toString(software.lastSetSource));
		context.write("set-count: ");
		writeUnsigned(context, software.setCount);
		context.writeLine("");
		context.write("network-sync-count: ");
		writeUnsigned(context, software.networkSyncCount);
		context.writeLine("");
		context.write("auto-network-sync: ");
		context.writeLine(software.autoNetworkSync ? "yes" : "no");
		context.write("network-server: ");
		context.writeLine(software.networkSyncServer != nullptr ? software.networkSyncServer : "-");
		context.write("network-timeout-ms: ");
		writeUnsigned(context, software.networkSyncTimeoutMs);
		context.writeLine("");
		context.write("network-interval-ms: ");
		writeUnsigned(context, software.networkSyncIntervalMs);
		context.writeLine("");
		context.write("network-retry-ms: ");
		writeUnsigned(context, software.networkSyncRetryIntervalMs);
		context.writeLine("");
		char timezone[10]{};
		detail::formatRtcTimezoneOffset(software.timezoneOffsetSeconds, timezone);
		context.write("timezone: ");
		context.writeLine(timezone);
		if (software.lastNetworkAttemptValid) {
			context.write("last-network-attempt: ");
			context.writeLine(software.lastNetworkAttemptOk ? "ok" : "failed");
			context.write("last-network-attempt-uptime-ms: ");
			writeUnsigned(context, software.lastNetworkAttemptUptimeMs);
			context.writeLine("");
		} else {
			context.writeLine("last-network-attempt: none");
		}
		if (software.lastNetworkSyncUptimeMs != 0U) {
			context.write("last-network-sync-uptime-ms: ");
			writeUnsigned(context, software.lastNetworkSyncUptimeMs);
			context.writeLine("");
		}
	}

} // namespace

ProgramDescriptor RtcStatusProgram::descriptor() {
	return {
	    .name = "rtc-status",
	    .help = "read RTC backend status",
	    .description = "Reads status from the selected RTC backend.",
	    .usage = "rtc-status [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>]",
	    .arguments = lexxos::apps::argumentList(detail::rtcCommonArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int RtcStatusProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	detail::RtcBackend backend{};
	if (!detail::openRtcBackend(context, "rtc-status", arguments, backend)) {
		return 1;
	}

	detail::RtcBackendStatus status{};
	if (!detail::readStatus(backend, status)) {
		context.writeErrorLine("rtc-status: failed to read RTC backend status");
		return 1;
	}

	context.write("source: ");
	context.writeLine(detail::backendName(backend.kind));
	if (status.rawValid) {
		context.write("status: ");
		detail::writeHex2(context, status.raw);
		context.writeLine("");
	}
	if (status.oscillatorStoppedValid) {
		context.write("oscillator stopped: ");
		context.writeLine(status.oscillatorStopped ? "yes" : "no");
	}
	if (status.voltageLowValid) {
		context.write("voltage low: ");
		context.writeLine(status.voltageLow ? "yes" : "no");
	}
	if (status.softwareStatusValid) {
		writeSoftwareStatus(context, status.softwareStatus);
	}
#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	if (status.jcsFirmwareValid) {
		context.write("firmware: type=");
		detail::writeHex2(context, status.jcsFirmware.type);
		context.write(" version=");
		writeUnsigned(context, status.jcsFirmware.majorVersion());
		context.write(".");
		writeUnsigned(context, status.jcsFirmware.minorVersion());
		context.writeLine(status.jcsFirmware.isJcs() ? " jcs=yes" : " jcs=no");
	}
	if (status.jcsRtcConfigValid) {
		context.write("rtc alarm: enabled=");
		context.write(status.jcsRtcConfig.alarmEnabled() ? "yes" : "no");
		context.write(" rearm=");
		context.writeLine(status.jcsRtcConfig.alarmRearm() ? "yes" : "no");
	}
#endif
	return 0;
}

} // namespace lexxos::apps::rtc_programs
