#include <lexxos/apps/network/hosted_ota_program.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <new>
#include <string_view>

#include <lexxos/app/console_input.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <esp_err.h>
	#include <esp_hosted.h>
	#include <esp_system.h>
	#include <lexxos/devices/espidf/espidf_hosted.h>
#endif

#include <lexxos/apps/network/detail/hosted_ota_support.h>

namespace lexxos::apps::network_programs {

using detail::hosted::allocateWorkspace;
using detail::hosted::hostedOtaWorkspaceBytes;
using detail::hosted::OtaBuffer;
using detail::hosted::PathBuffer;
using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;
#if defined(LEXXOS_PLATFORM_ESPIDF)
using detail::hosted::readFirstChunk;
using detail::hosted::streamFirmware;
using detail::hosted::toString;
using detail::hosted::writeEspError;
using detail::hosted::writeVersion;
#endif

namespace {

	constexpr ProgramArgumentSpec hostedOtaArguments[] = {
	    {
	     .name = "path",
	     .valueName = "path",
	     .help = "VFS path to ESP32-C6 network_adapter.bin",
	     .kind = ProgramArgumentKind::Positional,
	     .valueType = ProgramArgumentValueType::Path,
	     .required = true,
	     },
	    {
	     .name = "dry-run",
	     .help = "validate the source file and hosted link without writing C6 flash",
	     .kind = ProgramArgumentKind::Flag,
	     .required = false,
	     },
	    {
	     .name = "no-activate",
	     .help = "skip OTA activate even when the current C6 firmware supports it",
	     .kind = ProgramArgumentKind::Flag,
	     .required = false,
	     },
	    {
	     .name = "reboot-host",
	     .help = "reboot ESP32-P4 after a successful OTA",
	     .kind = ProgramArgumentKind::Flag,
	     .required = false,
	     },
	};

} // namespace

ProgramDescriptor HostedOtaProgram::descriptor() {
	return {
	    .name = "hosted-ota",
	    .help = "update ESP-Hosted C6 firmware from a VFS file",
	    .description = "Reads an ESP32-C6 network_adapter.bin from VFS and sends it to the ESP-Hosted coprocessor over the active board transport.",
	    .usage = "hosted-ota [--dry-run] [--no-activate] [--reboot-host] <path>",
	    .arguments = argumentList(hostedOtaArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = hostedOtaWorkspaceBytes,
	                .cancellable = false,
	                .allowBackground = false,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int HostedOtaProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	PathBuffer* path = nullptr;
	OtaBuffer* buffer = nullptr;
	if (!allocateWorkspace(context, path, buffer)) {
		return 1;
	}

	if (!resolvePath(context, arguments.positional("path"), *path)) {
		return 1;
	}

	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path->data(), lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("hosted-ota: ");
		context.write(path->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	auto const fileSize = file.size();
	if (fileSize == 0U) {
		context.writeLine("hosted-ota: source file is empty");
		return 1;
	}

	std::size_t firstBytesRead = 0;
	if (!readFirstChunk(context, file, path->data(), *buffer, firstBytesRead)) {
		return 1;
	}

	auto const& board = activeBoard(context);
	auto const info = lexxos::devices::espidf_hosted::readInfo(board, true);
	if (!info.supported) {
		context.writeLine("hosted-ota: active board does not use ESP-Hosted");
		return 1;
	}

	if (!info.backendStarted) {
		context.write("hosted-ota: hosted backend did not start: ");
		context.writeLine(info.backendStartErrorName);
		return 1;
	}

	context.write("hosted-ota: source=");
	context.write(path->data());
	context.write(" size=");
	writeUnsigned64(context, fileSize);
	context.writeLine(" bytes");
	context.write("hosted-ota: host-version=");
	writeVersion(context, info.hostVersion);
	context.write(" coprocessor-version=");
	writeVersion(context, info.coprocessorVersion);
	context.write(" compatible=");
	context.writeLine(info.compatibilityKnown ? (info.majorMinorCompatible ? "yes" : "no") : "unknown");

	if (arguments.has("dry-run")) {
		context.writeLine("hosted-ota: dry run passed");
		return 0;
	}

	context.writeLine("hosted-ota: begin");
	auto result = esp_hosted_slave_ota_begin();
	if (result != ESP_OK) {
		writeEspError(context, "hosted-ota: begin failed: ", result);
		return 1;
	}

	std::uint64_t bytesWritten = 0;
	if (!streamFirmware(context, file, path->data(), fileSize, *buffer, firstBytesRead, bytesWritten)) {
		return 1;
	}

	result = esp_hosted_slave_ota_end();
	if (result != ESP_OK) {
		writeEspError(context, "hosted-ota: end failed: ", result);
		return 1;
	}

	context.writeLine("hosted-ota: end ok");

	auto const canActivate = lexxos::devices::espidf_hosted::activateSupported(info.coprocessorVersion);
	if (canActivate && !arguments.has("no-activate")) {
		context.writeLine("hosted-ota: activate");
		result = esp_hosted_slave_ota_activate();
		if (result != ESP_OK) {
			writeEspError(context, "hosted-ota: activate failed: ", result);
			return 1;
		}

		context.writeLine("hosted-ota: activate ok");
	} else if (arguments.has("no-activate")) {
		context.writeLine("hosted-ota: activate skipped by --no-activate");
	} else {
		context.writeLine("hosted-ota: activate skipped; current C6 firmware is older than 2.6.0");
	}

	context.writeLine("hosted-ota: update completed");
	if (arguments.has("reboot-host")) {
		context.writeLine("hosted-ota: rebooting host");
		esp_restart();
	} else {
		context.writeLine("hosted-ota: reboot host before using Wi-Fi again");
	}

	return 0;
#else
	(void) arguments;
	context.writeErrorLine("hosted-ota: ESP-Hosted support is unavailable in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::network_programs
