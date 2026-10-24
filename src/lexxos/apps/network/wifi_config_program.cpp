#include <lexxos/apps/network/wifi_config_program.h>

#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/network/detail/wifi_credentials_store.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::network_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentValueType;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	constexpr std::uint32_t wifiConfigMaxTimeoutMs = 120000;

	constexpr lexxos::program::ProgramArgumentSpec wifiConfigArguments[] = {
	    {
         .name = "auth",
         .shortName = 'a',
         .valueName = "auto|open|wpa|wpa2|mixed",
         .help = "authentication mode to save",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "auto",
	     },
	    {
         .name = "timeout",
         .shortName = 't',
         .valueName = "ms",
         .help = "connection timeout to save",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "30000",
	     },
	    {
         .name = "action",
         .valueName = "set|show|clear",
         .help = "configuration action",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = true,
	     },
	    {
         .name = "ssid",
         .valueName = "ssid",
         .help = "Wi-Fi network SSID for set",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "password",
         .valueName = "password",
         .help = "Wi-Fi password for set",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .sensitive = true,
	     },
	};

	void writeStatus(lexxos::os::AppContext& context, detail::WifiCredentialsStatus status) {
		context.write(detail::toString(status));
	}

	bool parseTimeout(lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::uint32_t& output) {
		std::uint32_t timeoutMs = 30000;
		if (!parseUnsigned32(arguments.valueOr("timeout", "30000"), timeoutMs)
		    || timeoutMs == 0
		    || timeoutMs > wifiConfigMaxTimeoutMs) {
			context.writeErrorLine("wifi-config: invalid --timeout");
			return false;
		}
		output = timeoutMs;
		return true;
	}

	int showConfig(lexxos::os::AppContext& context) {
		detail::WifiCredentials credentials{};
		auto const status = detail::loadWifiCredentials(credentials);
		if (status == detail::WifiCredentialsStatus::NotFound) {
			context.write("wifi-config: not configured: ");
			context.writeLine(detail::wifiCredentialsPath());
			return 0;
		}
		if (status != detail::WifiCredentialsStatus::Ok && status != detail::WifiCredentialsStatus::Disabled) {
			context.write("wifi-config: failed to read config: ");
			writeStatus(context, status);
			context.writeLine("");
			return 1;
		}

		context.write("path: ");
		context.writeLine(detail::wifiCredentialsPath());
		context.write("enabled: ");
		context.writeLine(credentials.enabled ? "yes" : "no");
		context.write("ssid: ");
		context.writeLine(detail::wifiCredentialsSsid(credentials));
		context.write("auth: ");
		context.writeLine(detail::wifiCredentialsAuth(credentials));
		context.write("timeout: ");
		writeUnsigned(context, credentials.timeoutMs);
		context.writeLine(" ms");
		context.write("password: ");
		context.writeLine(detail::wifiCredentialsPassword(credentials).empty() ? "empty" : "set");
		return 0;
	}

	int setConfig(lexxos::os::AppContext& context, lexxos::program::ParsedArguments const& arguments) {
		auto const ssid = arguments.positional("ssid");
		auto const password = arguments.positionalOr("password", "");
		if (ssid.empty()) {
			context.writeErrorLine("wifi-config: missing SSID");
			context.writeErrorLine("usage: wifi-config set [--auth auto|open|wpa|wpa2|mixed] [--timeout <ms>] <ssid> [password]");
			return 2;
		}

		detail::WifiCredentials credentials{};
		credentials.enabled = true;
		std::uint32_t timeoutMs = 0;
		if (!parseTimeout(context, arguments, timeoutMs)) {
			return 2;
		}
		credentials.timeoutMs = timeoutMs;

		auto const auth = arguments.valueOr("auth", "auto");
		if (!detail::setWifiCredentialsSsid(credentials, ssid)) {
			context.writeErrorLine("wifi-config: SSID is too long");
			return 2;
		}
		if (!detail::setWifiCredentialsPassword(credentials, password)) {
			context.writeErrorLine("wifi-config: password is too long");
			return 2;
		}
		if (!detail::setWifiCredentialsAuth(credentials, auth)) {
			context.writeErrorLine("wifi-config: auth value is too long");
			return 2;
		}

		std::string_view error{};
		if (!detail::validateWifiCredentials(credentials, error)) {
			context.writeError("wifi-config: ");
			context.writeErrorLine(error);
			return 2;
		}

		auto const status = detail::saveWifiCredentials(credentials);
		if (status != detail::WifiCredentialsStatus::Ok) {
			context.writeError("wifi-config: failed to save config: ");
			context.writeErrorLine(detail::toString(status));
			return 1;
		}

		context.write("wifi-config: saved ");
		context.write(detail::wifiCredentialsSsid(credentials));
		context.write(" to ");
		context.writeLine(detail::wifiCredentialsPath());
		context.writeLine("wifi-config: warning: password is stored as plain text on the SD card");
		return 0;
	}

	int clearConfig(lexxos::os::AppContext& context) {
		auto const status = detail::clearWifiCredentials();
		if (status != detail::WifiCredentialsStatus::Ok) {
			context.writeError("wifi-config: failed to clear config: ");
			context.writeErrorLine(detail::toString(status));
			return 1;
		}
		context.writeLine("wifi-config: cleared");
		return 0;
	}

} // namespace

lexxos::program::ProgramDescriptor WifiConfigProgram::descriptor() {
	return {
	    .name = "wifi-config",
	    .help = "save Wi-Fi credentials for auto-connect",
	    .description = "Stores Wi-Fi credentials on the SD card and lets the boot auto-connect task use them.",
	    .usage = "wifi-config set [--auth auto|open|wpa|wpa2|mixed] [--timeout <ms>] <ssid> [password] | wifi-config show | wifi-config clear",
	    .arguments = argumentList(wifiConfigArguments),
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

int WifiConfigProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const action = arguments.positional("action");
	if (action == "set" || action == "save") {
		return setConfig(context, arguments);
	}
	if (action == "show" || action == "status") {
		return showConfig(context);
	}
	if (action == "clear" || action == "remove" || action == "forget") {
		return clearConfig(context);
	}

	context.writeErrorLine("wifi-config: unknown action, expected set, show or clear");
	context.writeErrorLine("usage: wifi-config set [--auth auto|open|wpa|wpa2|mixed] [--timeout <ms>] <ssid> [password] | wifi-config show | wifi-config clear");
	return 2;
}

} // namespace lexxos::apps::network_programs
