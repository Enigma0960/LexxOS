#include <lexxos/apps/network/wifi_scan_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/network/detail/wifi_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::network_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentValueType;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	constexpr lexxos::program::ProgramArgumentSpec wifiScanArguments[] = {
	    {
         .name = "timeout",
         .shortName = 't',
         .valueName = "ms",
         .help = "maximum time to wait for scan completion",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "10000",
	     },
	    {
         .name = "limit",
         .shortName = 'n',
         .valueName = "count",
         .help = "maximum network results to print",
         .kind = ProgramArgumentKind::Option,
         .valueType = ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "24",
	     },
	    {
         .name = "raw",
         .shortName = 'r',
         .help = "print raw backend scan records",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "bssid",
         .shortName = 'b',
         .help = "print access point BSSID",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor WifiScanProgram::descriptor() {
	return {
	    .name = "wifi-scan",
	    .help = "scan Wi-Fi networks",
	    .description = "Starts the board Wi-Fi station backend and prints visible Wi-Fi networks.",
	    .usage = "wifi-scan [--timeout <ms>] [--limit <count>] [--raw] [--bssid]",
	    .arguments = argumentList(wifiScanArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
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

int WifiScanProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	return detail::runWifiScan(context, arguments, activeBoard(context));
}

} // namespace lexxos::apps::network_programs
