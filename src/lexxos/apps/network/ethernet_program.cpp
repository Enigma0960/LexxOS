#include <lexxos/apps/network/ethernet_program.h>

#include <cstring>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/espidf/espidf_ethernet.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::network_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentList;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramArgumentValueType;
	using lexxos::program::ProgramDescriptor;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	template <std::size_t Size>
	constexpr ProgramArgumentList argumentList(ProgramArgumentSpec const (&arguments)[Size]) {
		return ProgramArgumentList{
		    .data = arguments,
		    .size = Size,
		};
	}

	constexpr ProgramArgumentSpec ethArguments[] = {
	    {
         .name = "action",
         .valueName = "status|start|stop|restart",
         .help = "Ethernet action, default status",
         .kind = ProgramArgumentKind::Positional,
         .valueType = ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	bool equals(std::string_view left, char const* right) {
		return left.size() == std::strlen(right) && std::memcmp(left.data(), right, left.size()) == 0;
	}

	void writeMac(lexxos::os::AppContext& context, std::uint8_t const mac[6]) {
		for (std::size_t index = 0; index < 6; ++index) {
			if (index != 0) {
				context.write(":");
			}
			writeHexByte(context, mac[index]);
		}
	}

	void writeStatus(lexxos::os::AppContext& context) {
		auto const status = lexxos::devices::espidf_ethernet::status();
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
	}

} // namespace

ProgramDescriptor EthernetProgram::descriptor() {
	return {
	    .name = "eth",
	    .help = "show or control wired Ethernet",
	    .description = "Starts, stops and reports the ESP-IDF Ethernet interface registered by the active board descriptor.",
	    .usage = "eth [status|start|stop|restart]",
	    .arguments = argumentList(ethArguments),
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

int EthernetProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const action = arguments.positionalOr("action", "status");
	auto const& board = activeBoard(context);
	bool ok = true;
	if (equals(action, "status")) {
		writeStatus(context);
		return 0;
	}
	if (equals(action, "start")) {
		ok = lexxos::devices::espidf_ethernet::start(board);
	} else if (equals(action, "stop")) {
		ok = lexxos::devices::espidf_ethernet::stop();
	} else if (equals(action, "restart")) {
		ok = lexxos::devices::espidf_ethernet::restart(board);
	} else {
		context.writeErrorLine("eth: action must be status, start, stop or restart");
		return 2;
	}

	if (!ok) {
		context.writeErrorLine("eth: action failed");
		writeStatus(context);
		return 1;
	}
	writeStatus(context);
	return 0;
}

} // namespace lexxos::apps::network_programs
