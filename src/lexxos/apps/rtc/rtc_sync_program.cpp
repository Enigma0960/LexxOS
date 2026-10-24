#include <lexxos/apps/rtc/rtc_sync_program.h>

#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/network/network_time.h>
#include <lexxos/apps/rtc/detail/rtc_program_support.h>
#include <lexxos/apps/rtc/detail/rtc_settings_store.h>
#include <lexxos/devices/rtc/software_rtc.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::rtc_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentValueType;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;
	using DateTime = lexxos::devices::rtc::DateTime;

	constexpr std::uint32_t rtcSyncDefaultTimeoutMs = 8000;
	constexpr std::uint32_t rtcSyncMaxTimeoutMs = 60000;

	void writeOffset2(lexxos::os::AppContext& context, std::uint32_t value) {
		char buffer[3]{static_cast<char>('0' + (value / 10U)), static_cast<char>('0' + (value % 10U)), '\0'};
		context.write(buffer);
	}

	void writeTimezoneOffset(lexxos::os::AppContext& context, std::int32_t offsetSeconds) {
		context.write("UTC");
		if (offsetSeconds == 0) {
			return;
		}
		context.write(offsetSeconds < 0 ? "-" : "+");
		std::uint32_t const absolute = static_cast<std::uint32_t>(offsetSeconds < 0 ? -offsetSeconds : offsetSeconds);
		writeOffset2(context, absolute / 3600U);
		context.write(":");
		writeOffset2(context, (absolute / 60U) % 60U);
	}

	void formatAddress(std::uint8_t value, char (&output)[5]) {
		constexpr char digits[] = "0123456789ABCDEF";
		output[0] = '0';
		output[1] = 'x';
		output[2] = digits[(value >> 4U) & 0x0FU];
		output[3] = digits[value & 0x0FU];
		output[4] = '\0';
	}

	bool loadSettingsForSync(lexxos::os::AppContext& context, detail::RtcSettings& settings) {
		auto const status = detail::loadRtcSettings(settings, lexxos::devices::rtc::software::config(context.platform()));
		if (status == detail::RtcSettingsStatus::Ok || status == detail::RtcSettingsStatus::NotFound) {
			return true;
		}
		context.write("rtc-sync: warning: ignored invalid RTC config: ");
		context.writeLine(detail::toString(status));
		detail::resetRtcSettingsDefaults(settings, lexxos::devices::rtc::software::config(context.platform()));
		return true;
	}

	bool applySyncArguments(
	    lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    detail::RtcSettings& settings) {
		if (arguments.has("server") && !detail::setRtcSettingsServer(settings, arguments.value("server"))) {
			context.writeErrorLine("rtc-sync: invalid --server");
			return false;
		}
		if (arguments.has("timeout")) {
			std::uint32_t timeoutMs = rtcSyncDefaultTimeoutMs;
			if (!parseUnsigned32(arguments.value("timeout"), timeoutMs)
			    || timeoutMs == 0
			    || timeoutMs > rtcSyncMaxTimeoutMs) {
				context.writeErrorLine("rtc-sync: invalid --timeout");
				return false;
			}
			settings.timeoutMs = timeoutMs;
		}
		if (arguments.has("tz") && !detail::parseRtcTimezoneOffset(arguments.value("tz"), settings.timezoneOffsetSeconds)) {
			context.writeErrorLine("rtc-sync: invalid --tz, expected UTC, Z, +HH, +HH:MM, +HHMM, -HH, -HH:MM or -HHMM");
			return false;
		}
		if (arguments.has("source") && !detail::setRtcSettingsSource(settings, arguments.value("source"))) {
			context.writeErrorLine("rtc-sync: invalid --source value, expected auto, software, picocalc, ds3231, pcf8563 or bm8563");
			return false;
		}
		if (arguments.has("bus") && !detail::setRtcSettingsBus(settings, arguments.value("bus"))) {
			context.writeErrorLine("rtc-sync: invalid --bus");
			return false;
		}
		if (arguments.has("address")) {
			std::uint8_t address = 0;
			if (!detail::parseRtcI2cAddress(arguments.value("address"), address)) {
				context.writeErrorLine("rtc-sync: invalid --address value");
				return false;
			}
			settings.addressSet = true;
			settings.address = address;
		}

		std::string_view error{};
		if (!detail::validateRtcSettings(settings, error)) {
			context.writeError("rtc-sync: ");
			context.writeErrorLine(error);
			return false;
		}
		return true;
	}

	bool openConfiguredBackend(lexxos::os::AppContext& context, detail::RtcSettings const& settings, detail::RtcBackend& backend) {
		lexxos::program::ParsedArguments effectiveArguments{};
		(void) effectiveArguments.setNamed("source", detail::rtcSettingsSource(settings), true);
		(void) effectiveArguments.setNamed("bus", detail::rtcSettingsBus(settings), true);
		char addressText[5]{};
		if (settings.addressSet) {
			formatAddress(settings.address, addressText);
			(void) effectiveArguments.setNamed("address", addressText, true);
		}
		return detail::openRtcBackend(context, "rtc-sync", effectiveArguments, backend);
	}

	void saveSettingsAfterSync(lexxos::os::AppContext& context, detail::RtcSettings const& settings) {
		auto const status = detail::saveRtcSettings(settings);
		if (status != detail::RtcSettingsStatus::Ok) {
			context.write("rtc-sync: warning: failed to save RTC settings: ");
			context.writeLine(detail::toString(status));
			return;
		}
		auto const config = detail::softwareRtcConfigFromSettings(settings);
		(void) lexxos::devices::rtc::software::applyRuntimeConfigOverride(
		    context.platform(),
		    config,
		    detail::rtcSettingsServer(settings));
	}

	constexpr lexxos::program::ProgramArgumentSpec rtcSyncArguments[] = {
	    {
         .name = "server",
         .shortName = 'n',
         .valueName = "host",
         .help = "NTP server host name",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "pool.ntp.org",
	     },
	    {
         .name = "timeout",
         .shortName = 't',
         .valueName = "ms",
         .help = "overall DNS and NTP timeout",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "8000",
	     },
	    {
         .name = "tz",
         .shortName = 'z',
         .valueName = "+HH[:MM]|-HH[:MM]|UTC",
         .help = "timezone offset applied before writing RTC",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "UTC",
	     },
	    {
         .name = "source",
         .shortName = 's',
         .valueName = "auto|software|picocalc|ds3231|pcf8563",
         .help = "RTC backend source",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "auto",
	     },
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
         .help = "RTC 7-bit I2C address, decimal or hex; backend default when omitted",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor RtcSyncProgram::descriptor() {
	return {
	    .name = "rtc-sync",
	    .help = "synchronize RTC from an NTP server",
	    .description = "Fetches UTC time from an NTP server over the active network interface and writes UTC or timezone-adjusted time to the selected RTC backend.",
	    .usage = "rtc-sync [--server <host>] [--timeout <ms>] [--tz <offset>] [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>]",
	    .arguments = argumentList(rtcSyncArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 6144,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int RtcSyncProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	detail::RtcSettings settings{};
	if (!loadSettingsForSync(context, settings) || !applySyncArguments(context, arguments, settings)) {
		return 2;
	}

	lexxos::devices::rtc::DateTime networkTime{};
	std::uint32_t unixSeconds = 0;
	if (!lexxos::apps::network_programs::queryNetworkTime(
	        context,
	        detail::rtcSettingsServer(settings),
	        settings.timeoutMs,
	        networkTime,
	        unixSeconds)) {
		return 1;
	}

	DateTime rtcTime{};
	std::int64_t const adjustedUnixSeconds = static_cast<std::int64_t>(unixSeconds) + static_cast<std::int64_t>(settings.timezoneOffsetSeconds);
	if (adjustedUnixSeconds < 0
	    || !lexxos::devices::rtc::unixSecondsToDateTime(static_cast<std::uint64_t>(adjustedUnixSeconds), rtcTime)) {
		context.writeErrorLine("rtc-sync: timezone-adjusted time is outside the supported RTC range");
		return 1;
	}

	detail::RtcBackend backend{};
	if (!openConfiguredBackend(context, settings, backend)) {
		return 1;
	}

	if (!detail::writeDateTime(
	        backend,
	        rtcTime,
	        uptimeMs(context),
	        lexxos::devices::rtc::software::SetSource::Network)) {
		context.writeErrorLine("rtc-sync: failed to write RTC date/time");
		return 1;
	}

	context.write("source: ");
	context.writeLine(detail::backendName(backend.kind));
	context.write("network time: ");
	detail::writeDateTime(context, networkTime);
	context.writeLine(" UTC");
	context.write("rtc time: ");
	detail::writeDateTime(context, rtcTime);
	context.write(" ");
	writeTimezoneOffset(context, settings.timezoneOffsetSeconds);
	context.writeLine("");
	context.write("unix: ");
	writeUnsigned(context, unixSeconds);
	context.writeLine("");
	saveSettingsAfterSync(context, settings);
	context.writeLine("rtc sync: ok");
	return 0;
}

} // namespace lexxos::apps::rtc_programs
