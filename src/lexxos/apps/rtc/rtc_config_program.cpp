#include <lexxos/apps/rtc/rtc_config_program.h>

#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/rtc/detail/rtc_settings_store.h>
#include <lexxos/devices/rtc/software_rtc.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::rtc_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentValueType;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	constexpr std::uint32_t rtcConfigMaxTimeoutMs = 60000;

	constexpr lexxos::program::ProgramArgumentSpec rtcConfigArguments[] = {
	    {
         .name = "enabled",
         .valueName = "yes|no",
         .help = "enable software RTC runtime backend",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "auto-sync",
         .valueName = "yes|no",
         .help = "enable automatic NTP sync",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "prefer-default",
         .valueName = "yes|no",
         .help = "prefer software RTC for --source auto",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "server",
         .shortName = 'n',
         .valueName = "host",
         .help = "NTP server host name",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "timeout",
         .shortName = 't',
         .valueName = "ms",
         .help = "overall DNS and NTP timeout",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "interval",
         .valueName = "ms",
         .help = "successful NTP sync interval",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "retry",
         .valueName = "ms",
         .help = "failed NTP sync retry interval",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "startup-delay",
         .valueName = "ms",
         .help = "software RTC task startup delay",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
	     },
	    {
         .name = "tz",
         .shortName = 'z',
         .valueName = "+HH[:MM]|-HH[:MM]|UTC",
         .help = "timezone offset applied before writing RTC",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "source",
         .shortName = 's',
         .valueName = "auto|software|picocalc|ds3231|pcf8563",
         .help = "default rtc-sync backend source",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "bus",
         .shortName = 'b',
         .valueName = "name",
         .help = "default I2C bus for external RTC",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "address",
         .shortName = 'a',
         .valueName = "addr|-",
         .help = "default RTC I2C address or - to use backend default",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "action",
         .valueName = "show|set|clear",
         .help = "configuration action",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = true,
	     },
	};

	void writeBool(lexxos::os::AppContext& context, bool value) {
		context.writeLine(value ? "yes" : "no");
	}

	bool parseBoolean(std::string_view text, bool& output) {
		if (text == "1" || text == "true" || text == "yes" || text == "on") {
			output = true;
			return true;
		}
		if (text == "0" || text == "false" || text == "no" || text == "off") {
			output = false;
			return true;
		}
		return false;
	}

	bool applyBooleanOption(
	    lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view name,
	    bool& output) {
		if (!arguments.has(name)) {
			return true;
		}
		bool value = false;
		if (!parseBoolean(arguments.value(name), value)) {
			context.writeError("rtc-config: invalid --");
			context.writeError(name);
			context.writeErrorLine(", expected yes or no");
			return false;
		}
		output = value;
		return true;
	}

	bool applyUnsignedOption(
	    lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view name,
	    std::uint32_t maxValue,
	    std::uint32_t& output) {
		if (!arguments.has(name)) {
			return true;
		}
		std::uint32_t value = 0;
		if (!parseUnsigned32(arguments.value(name), value) || value > maxValue) {
			context.writeError("rtc-config: invalid --");
			context.writeError(name);
			context.writeErrorLine("");
			return false;
		}
		output = value;
		return true;
	}

	bool loadCurrentSettings(lexxos::os::AppContext& context, detail::RtcSettings& settings, detail::RtcSettingsStatus& status) {
		status = detail::loadRtcSettings(settings, lexxos::devices::rtc::software::config(context.platform()));
		if (status == detail::RtcSettingsStatus::Ok || status == detail::RtcSettingsStatus::NotFound) {
			return true;
		}
		context.writeError("rtc-config: failed to read config: ");
		context.writeErrorLine(detail::toString(status));
		return false;
	}

	void writeAddress(lexxos::os::AppContext& context, detail::RtcSettings const& settings) {
		if (!settings.addressSet) {
			context.writeLine("-");
			return;
		}
		writeHex8(context, settings.address);
		context.writeLine("");
	}

	void writeSettings(
	    lexxos::os::AppContext& context,
	    detail::RtcSettings const& settings,
	    detail::RtcSettingsStatus loadStatus) {
		char timezone[10]{};
		detail::formatRtcTimezoneOffset(settings.timezoneOffsetSeconds, timezone);
		context.write("path: ");
		context.writeLine(detail::rtcSettingsPath);
		context.write("configured: ");
		context.writeLine(loadStatus == detail::RtcSettingsStatus::Ok ? "yes" : "no");
		context.write("runtime-override: ");
		context.writeLine(lexxos::devices::rtc::software::runtimeConfigOverrideActive() ? "yes" : "no");
		context.write("enabled: ");
		writeBool(context, settings.enabled);
		context.write("prefer-default: ");
		writeBool(context, settings.preferAsDefault);
		context.write("auto-sync: ");
		writeBool(context, settings.autoNetworkSync);
		context.write("server: ");
		context.writeLine(detail::rtcSettingsServer(settings));
		context.write("timeout-ms: ");
		writeUnsigned(context, settings.timeoutMs);
		context.writeLine("");
		context.write("interval-ms: ");
		writeUnsigned(context, settings.intervalMs);
		context.writeLine("");
		context.write("retry-ms: ");
		writeUnsigned(context, settings.retryMs);
		context.writeLine("");
		context.write("startup-delay-ms: ");
		writeUnsigned(context, settings.startupDelayMs);
		context.writeLine("");
		context.write("tz: ");
		context.writeLine(timezone);
		context.write("source: ");
		context.writeLine(detail::rtcSettingsSource(settings));
		context.write("bus: ");
		context.writeLine(detail::rtcSettingsBus(settings));
		context.write("address: ");
		writeAddress(context, settings);
	}

	void applyRuntimeOverride(lexxos::os::AppContext& context, detail::RtcSettings const& settings) {
		auto const config = detail::softwareRtcConfigFromSettings(settings);
		if (!lexxos::devices::rtc::software::applyRuntimeConfigOverride(
		        context.platform(),
		        config,
		        detail::rtcSettingsServer(settings))) {
			context.writeLine("rtc-config: warning: software RTC runtime override was not applied");
		}
	}

	int showConfig(lexxos::os::AppContext& context) {
		detail::RtcSettings settings{};
		detail::RtcSettingsStatus status{};
		if (!loadCurrentSettings(context, settings, status)) {
			return 1;
		}
		writeSettings(context, settings, status);
		return 0;
	}

	int setConfig(lexxos::os::AppContext& context, lexxos::program::ParsedArguments const& arguments) {
		detail::RtcSettings settings{};
		detail::RtcSettingsStatus loadStatus{};
		if (!loadCurrentSettings(context, settings, loadStatus)) {
			return 1;
		}

		if (!applyBooleanOption(context, arguments, "enabled", settings.enabled)
		    || !applyBooleanOption(context, arguments, "auto-sync", settings.autoNetworkSync)
		    || !applyBooleanOption(context, arguments, "prefer-default", settings.preferAsDefault)
		    || !applyUnsignedOption(context, arguments, "timeout", rtcConfigMaxTimeoutMs, settings.timeoutMs)
		    || !applyUnsignedOption(context, arguments, "interval", 0xFFFFFFFFU, settings.intervalMs)
		    || !applyUnsignedOption(context, arguments, "retry", 0xFFFFFFFFU, settings.retryMs)
		    || !applyUnsignedOption(context, arguments, "startup-delay", 0xFFFFFFFFU, settings.startupDelayMs)) {
			return 2;
		}

		if (arguments.has("server") && !detail::setRtcSettingsServer(settings, arguments.value("server"))) {
			context.writeErrorLine("rtc-config: invalid --server");
			return 2;
		}
		if (arguments.has("tz") && !detail::parseRtcTimezoneOffset(arguments.value("tz"), settings.timezoneOffsetSeconds)) {
			context.writeErrorLine("rtc-config: invalid --tz");
			return 2;
		}
		if (arguments.has("source") && !detail::setRtcSettingsSource(settings, arguments.value("source"))) {
			context.writeErrorLine("rtc-config: invalid --source");
			return 2;
		}
		if (arguments.has("bus") && !detail::setRtcSettingsBus(settings, arguments.value("bus"))) {
			context.writeErrorLine("rtc-config: invalid --bus");
			return 2;
		}
		if (arguments.has("address")) {
			auto const addressText = arguments.value("address");
			if (addressText == "-" || addressText.empty()) {
				settings.addressSet = false;
				settings.address = 0;
			} else {
				std::uint8_t address = 0;
				if (!detail::parseRtcI2cAddress(addressText, address)) {
					context.writeErrorLine("rtc-config: invalid --address");
					return 2;
				}
				settings.addressSet = true;
				settings.address = address;
			}
		}

		std::string_view error{};
		if (!detail::validateRtcSettings(settings, error)) {
			context.writeError("rtc-config: ");
			context.writeErrorLine(error);
			return 2;
		}

		auto const status = detail::saveRtcSettings(settings);
		if (status != detail::RtcSettingsStatus::Ok) {
			context.writeError("rtc-config: failed to save config: ");
			context.writeErrorLine(detail::toString(status));
			return 1;
		}
		applyRuntimeOverride(context, settings);
		context.write("rtc-config: saved to ");
		context.writeLine(detail::rtcSettingsPath);
		return 0;
	}

	int clearConfig(lexxos::os::AppContext& context) {
		auto const status = detail::clearRtcSettings();
		if (status != detail::RtcSettingsStatus::Ok) {
			context.writeError("rtc-config: failed to clear config: ");
			context.writeErrorLine(detail::toString(status));
			return 1;
		}
		lexxos::devices::rtc::software::clearRuntimeConfigOverride();
		context.writeLine("rtc-config: cleared");
		return 0;
	}

} // namespace

lexxos::program::ProgramDescriptor RtcConfigProgram::descriptor() {
	return {
	    .name = "rtc-config",
	    .help = "save RTC settings for boot restore",
	    .description = "Stores RTC/NTP settings on the SD card and applies them to software RTC auto-sync.",
	    .usage = "rtc-config show | rtc-config set [--enabled yes|no] [--auto-sync yes|no] [--prefer-default yes|no] [--server <host>] [--timeout <ms>] [--interval <ms>] [--retry <ms>] [--startup-delay <ms>] [--tz <offset>] [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>|-] | rtc-config clear",
	    .arguments = lexxos::apps::argumentList(rtcConfigArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
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

int RtcConfigProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const action = arguments.positional("action");
	if (action == "show" || action == "status") {
		return showConfig(context);
	}
	if (action == "set" || action == "save") {
		return setConfig(context, arguments);
	}
	if (action == "clear" || action == "remove" || action == "forget") {
		return clearConfig(context);
	}

	context.writeErrorLine("rtc-config: unknown action, expected show, set or clear");
	context.writeErrorLine("usage: rtc-config show | rtc-config set [options] | rtc-config clear");
	return 2;
}

} // namespace lexxos::apps::rtc_programs
