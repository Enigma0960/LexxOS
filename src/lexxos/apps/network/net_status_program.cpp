#include <lexxos/apps/network/net_status_program.h>

#include <cstddef>
#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/network/detail/wifi_program_support.h>
#include <lexxos/os/app_context.h>

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <lexxos/devices/espidf/espidf_ethernet.h>
#endif

namespace lexxos::apps::network_programs {
namespace {

	using lexxos::program::ProgramDescriptor;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

#if defined(LEXXOS_PLATFORM_ESPIDF)
	void writeMac(lexxos::os::AppContext& context, std::uint8_t const mac[6]) {
		for (std::size_t index = 0; index < 6; ++index) {
			if (index != 0) {
				context.write(":");
			}
			writeHexByte(context, mac[index]);
		}
	}

	bool writeEspIdfEthernetStatus(lexxos::os::AppContext& context) {
		auto const status = lexxos::devices::espidf_ethernet::status();
		context.writeLine("interface: ethernet");
		context.write("supported: ");
		context.writeLine(status.supported ? "yes" : "no");
		context.write("initialized: ");
		context.writeLine(status.initialized ? "yes" : "no");
		context.write("started: ");
		context.writeLine(status.started ? "yes" : "no");
		context.write("link: ");
		context.writeLine(status.linkUp ? "up" : "down");
		context.write("mac: ");
		if (status.macValid) {
			writeMac(context, status.mac);
			context.writeLine("");
		} else {
			context.writeLine("-");
		}
		context.write("hostname: ");
		context.writeLine(status.hostname[0] != '\0' ? status.hostname : "-");
		context.write("speed: ");
		if (status.speedValid) {
			writeUnsigned(context, status.speedMbps);
			context.writeLine(" Mbps");
		} else {
			context.writeLine("-");
		}
		context.write("duplex: ");
		context.writeLine(status.duplexValid ? (status.fullDuplex ? "full" : "half") : "-");
		context.write("ip: ");
		context.writeLine(status.gotIp ? status.ip : "-");
		context.write("netmask: ");
		context.writeLine(status.gotIp ? status.netmask : "-");
		context.write("gateway: ");
		context.writeLine(status.gotIp ? status.gateway : "-");
		context.write("last-error: ");
		context.writeLine(status.lastErrorName[0] != '\0' ? status.lastErrorName : "-");
		return status.supported && status.initialized && status.started && status.linkUp && status.gotIp;
	}

	int runEspIdfNetworkStatus(lexxos::os::AppContext& context, lexxos::platform::BoardDescriptor const& board) {
		bool anyReady = false;
		bool wroteAny = false;
		if (board.ethernet.enabled) {
			anyReady = writeEspIdfEthernetStatus(context) || anyReady;
			wroteAny = true;
		}
		if (board.wireless.wifi) {
			if (wroteAny) {
				context.writeLine("");
			}
			anyReady = detail::runWifiStatus(context, lexxos::program::ParsedArguments{}, board) == 0 || anyReady;
			wroteAny = true;
		}
		if (!wroteAny) {
			context.writeErrorLine("net-status: no network interface is declared by this board profile");
		}
		return anyReady ? 0 : 1;
	}
#endif

} // namespace

ProgramDescriptor NetStatusProgram::descriptor() {
	return {
	    .name = "net-status",
	    .help = "show active network interface status",
	    .description = "Reports the active board network interface using the platform network backend.",
	    .usage = "net-status",
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

int NetStatusProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	(void) arguments;
	return runEspIdfNetworkStatus(context, activeBoard(context));
#elif defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	return detail::runWifiStatus(context, arguments, activeBoard(context));
#else
	(void) arguments;
	context.writeErrorLine("net-status: network status support is unavailable in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::network_programs
