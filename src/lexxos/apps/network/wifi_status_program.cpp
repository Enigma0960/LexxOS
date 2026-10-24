#include <lexxos/apps/network/wifi_status_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/network/detail/wifi_program_support.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::network_programs {

lexxos::program::ProgramDescriptor WifiStatusProgram::descriptor() {
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	return {
	    .name = "wifi-status",
	    .help = "show Wi-Fi link and IP status",
	    .description = "Prints station mode, link status, IPv4 address, gateway and DNS servers.",
	    .usage = "wifi-status",
	    .arguments = {},
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

int WifiStatusProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	return detail::runWifiStatus(context, arguments, activeBoard(context));
}

} // namespace lexxos::apps::network_programs
