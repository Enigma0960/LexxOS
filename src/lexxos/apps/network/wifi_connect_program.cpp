#include <lexxos/apps/network/wifi_connect_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/network/detail/wifi_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::network_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentValueType;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	constexpr lexxos::program::ProgramArgumentSpec wifiConnectArguments[] = {
	    {
         .name = "timeout",
         .shortName = 't',
         .valueName = "ms",
         .help = "maximum time to wait for connection",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "30000",
	     },
	    {
         .name = "auth",
         .shortName = 'a',
         .valueName = "auto|open|wpa|wpa2|mixed",
         .help = "Wi-Fi authentication mode",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "auto",
	     },
	    {
         .name = "ssid",
         .valueName = "ssid",
         .help = "Wi-Fi network SSID",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = true,
	     },
	    {
         .name = "password",
         .valueName = "password",
         .help = "Wi-Fi network password",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
         .sensitive = true,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor WifiConnectProgram::descriptor() {
	return {
	    .name = "wifi-connect",
	    .help = "connect to a Wi-Fi network",
	    .description = "Starts the board Wi-Fi station backend and connects it to the selected network.",
	    .usage = "wifi-connect [--timeout <ms>] [--auth auto|open|wpa|wpa2|mixed] <ssid> [password]",
	    .arguments = argumentList(wifiConnectArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .cancellable = true,
	                .allowBackground = true,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int WifiConnectProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	return detail::runWifiConnect(context, arguments, activeBoard(context));
}

} // namespace lexxos::apps::network_programs
