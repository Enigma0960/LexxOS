#include <lexxos/apps/network/hosted_info_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <lexxos/devices/espidf/espidf_hosted.h>
#endif

namespace lexxos::apps::network_programs {
namespace {

	using lexxos::program::ProgramArgumentKind;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::program::ProgramDescriptor;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	constexpr ProgramArgumentSpec hostedInfoArguments[] = {
	    {
         .name = "no-start",
         .help = "do not start the hosted Wi-Fi backend before querying the coprocessor",
         .kind = ProgramArgumentKind::Flag,
         .required = false,
	     },
	};

#if defined(LEXXOS_PLATFORM_ESPIDF)
	void writeVersion(lexxos::os::AppContext& context, lexxos::devices::espidf_hosted::Version version) {
		if (!version.valid) {
			context.write("-");
			return;
		}
		writeUnsigned(context, version.major);
		context.write(".");
		writeUnsigned(context, version.minor);
		context.write(".");
		writeUnsigned(context, version.patch);
	}

	void writeSdioSummary(lexxos::os::AppContext& context, lexxos::platform::SdioPins const& pins) {
		if (!pins.isValid()) {
			context.writeLine("transport: -");
			return;
		}
		context.write("transport: sdio index=");
		writeUnsigned(context, static_cast<std::uint32_t>(pins.index));
		context.write(" clk=");
		writePin(context, pins.clock);
		context.write(" cmd=");
		writePin(context, pins.command);
		context.write(" d0=");
		writePin(context, pins.data0);
		context.write(" d1=");
		writePin(context, pins.data1);
		context.write(" d2=");
		writePin(context, pins.data2);
		context.write(" d3=");
		writePin(context, pins.data3);
		context.write(" xfer-hz=");
		writeUnsigned(context, pins.transferClockHz);
		context.writeLine("");
	}

	void writeAppField(lexxos::os::AppContext& context, char const* label, char const* value) {
		context.write(label);
		context.write(value != nullptr && value[0] != '\0' ? value : "-");
		context.writeLine("");
	}

	void writeHostedInfo(lexxos::os::AppContext& context, lexxos::platform::BoardDescriptor const& board,
	    lexxos::devices::espidf_hosted::Info const& info) {
		context.write("supported: ");
		context.writeLine(info.supported ? "yes" : "no");
		context.write("backend: ");
		context.writeLine(board.wireless.backend != nullptr ? board.wireless.backend : "-");
		context.write("radio: ");
		context.writeLine(board.wireless.radio != nullptr ? board.wireless.radio : "-");
		writeSdioSummary(context, board.wireless.hostedSdio);
		context.write("reset: ");
		writePin(context, board.wireless.hostedReset);
		context.writeLine("");
		context.write("backend-start-requested: ");
		context.writeLine(info.backendStartRequested ? "yes" : "no");
		context.write("backend-started: ");
		context.write(info.backendStarted ? "yes" : "no");
		context.write(" error=");
		context.writeLine(info.backendStartErrorName);
		context.write("host-version: ");
		writeVersion(context, info.hostVersion);
		context.writeLine("");
		context.write("coprocessor-version: ");
		writeVersion(context, info.coprocessorVersion);
		context.write(" error=");
		context.writeLine(info.versionErrorName);
		context.write("major-minor-compatible: ");
		context.writeLine(info.compatibilityKnown ? (info.majorMinorCompatible ? "yes" : "no") : "unknown");
		context.write("version-order: ");
		if (!info.compatibilityKnown) {
			context.writeLine("unknown");
		} else if (info.versionComparison == 0) {
			context.writeLine("same");
		} else if (info.versionComparison > 0) {
			context.writeLine("host-newer");
		} else {
			context.writeLine("coprocessor-newer");
		}
		context.write("activate-supported: ");
		context.writeLine(lexxos::devices::espidf_hosted::activateSupported(info.coprocessorVersion) ? "yes" : "no");
		context.write("app-desc: ");
		context.write(info.app.valid ? "yes" : "no");
		context.write(" error=");
		context.writeLine(info.appErrorName);
	}
#endif

} // namespace

ProgramDescriptor HostedInfoProgram::descriptor() {
	return {
	    .name = "hosted-info",
	    .help = "show ESP-Hosted coprocessor information",
	    .description = "Starts the ESP-Hosted transport if needed and prints host/slave firmware versions and app descriptor fields.",
	    .usage = "hosted-info [--no-start]",
	    .arguments = argumentList(hostedInfoArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 5120,
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

int HostedInfoProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	auto const& board = activeBoard(context);
	auto const info = lexxos::devices::espidf_hosted::readInfo(board, !arguments.has("no-start"));
	writeHostedInfo(context, board, info);
	if (!info.supported) {
		return 1;
	}
	if (info.app.valid) {
		writeAppField(context, "app-name: ", info.app.projectName);
		writeAppField(context, "app-version: ", info.app.version);
		writeAppField(context, "app-idf: ", info.app.idfVersion);
		writeAppField(context, "app-date: ", info.app.buildDate);
		writeAppField(context, "app-time: ", info.app.buildTime);
		context.write("secure-version: ");
		writeUnsigned(context, info.app.secureVersion);
		context.writeLine("");
	}
	return info.coprocessorVersion.valid || info.app.valid ? 0 : 1;
#else
	(void) arguments;
	context.writeErrorLine("hosted-info: ESP-Hosted support is unavailable in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::network_programs
