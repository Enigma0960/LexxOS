#include <lexxos/apps/network/hosted_ota_rx_program.h>

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

using detail::hosted::OtaBuffer;
using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;
#if defined(LEXXOS_PLATFORM_ESPIDF)
using detail::hosted::OtaRxStatus;
using detail::hosted::receiveYmodemHostedOta;
using detail::hosted::toString;
using detail::hosted::writeEspError;
using detail::hosted::writeVersion;
#endif

namespace {

	constexpr ProgramArgumentSpec hostedOtaRxArguments[] = {
	    {
	     .name = "dry-run",
	     .help = "receive and validate the YMODEM image without writing C6 flash",
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

ProgramDescriptor HostedOtaRxProgram::descriptor() {
	return {
	    .name = "hosted-ota-rx",
	    .help = "update ESP-Hosted C6 firmware from UART/YMODEM",
	    .description = "Receives an ESP32-C6 network_adapter.bin over the raw console with YMODEM and streams it directly to the ESP-Hosted coprocessor OTA RPC.",
	    .usage = "hosted-ota-rx [--dry-run] [--no-activate] [--reboot-host]",
	    .arguments = argumentList(hostedOtaRxArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = static_cast<std::uint32_t>(sizeof(OtaBuffer) + alignof(OtaBuffer)),
	                .cancellable = false,
	                .allowBackground = false,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int HostedOtaRxProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
#if defined(LEXXOS_PLATFORM_ESPIDF)
	auto const dryRun = arguments.has("dry-run");

	auto const& board = activeBoard(context);
	auto const info = lexxos::devices::espidf_hosted::readInfo(board, true);
	if (!info.supported) {
		context.writeLine("hosted-ota-rx: active board does not use ESP-Hosted");
		return 1;
	}

	if (!info.backendStarted) {
		context.write("hosted-ota-rx: hosted backend did not start: ");
		context.writeLine(info.backendStartErrorName);
		return 1;
	}

	context.write("hosted-ota-rx: host-version=");
	writeVersion(context, info.hostVersion);
	context.write(" coprocessor-version=");
	writeVersion(context, info.coprocessorVersion);
	context.write(" compatible=");
	context.writeLine(info.compatibilityKnown ? (info.majorMinorCompatible ? "yes" : "no") : "unknown");

	if (!dryRun) {
		context.writeLine("hosted-ota-rx: begin");
		auto const beginResult = esp_hosted_slave_ota_begin();
		if (beginResult != ESP_OK) {
			writeEspError(context, "hosted-ota-rx: begin failed: ", beginResult);
			return 1;
		}
	}

	context.writeLine("hosted-ota-rx: waiting for YMODEM sender");
	context.writeLine("hosted-ota-rx: host example: ymodem.send_file(serial, network_adapter.bin)");
	auto const receiveResult = receiveYmodemHostedOta(context, dryRun);
	if (receiveResult.status != OtaRxStatus::Ok) {
		context.write("hosted-ota-rx: ");
		context.write(toString(receiveResult.status));
		if (receiveResult.status == OtaRxStatus::InvalidImageMagic) {
			context.write(": 0x");
			writeHexByte(context, receiveResult.firstByte);
			context.write(", expected 0xE9");
		} else if (receiveResult.status == OtaRxStatus::OtaWriteFailed) {
			context.write(": ");
			context.write(esp_err_to_name(receiveResult.otaError));
		}

		context.writeLine("");
		return 1;
	}

	context.write("hosted-ota-rx: received ");
	writeUnsigned64(context, receiveResult.bytes);
	if (receiveResult.hasExpectedBytes) {
		context.write("/");
		writeUnsigned64(context, receiveResult.expectedBytes);
	}

	context.writeLine(" bytes");
	if (receiveResult.hasExpectedBytes && receiveResult.bytes != receiveResult.expectedBytes) {
		context.writeLine("hosted-ota-rx: received byte count does not match YMODEM metadata");
		return 1;
	}

	if (dryRun) {
		context.writeLine("hosted-ota-rx: dry run passed");
		return 0;
	}

	auto result = esp_hosted_slave_ota_end();
	if (result != ESP_OK) {
		writeEspError(context, "hosted-ota-rx: end failed: ", result);
		return 1;
	}

	context.writeLine("hosted-ota-rx: end ok");

	auto const canActivate = lexxos::devices::espidf_hosted::activateSupported(info.coprocessorVersion);
	if (canActivate && !arguments.has("no-activate")) {
		context.writeLine("hosted-ota-rx: activate");
		result = esp_hosted_slave_ota_activate();
		if (result != ESP_OK) {
			writeEspError(context, "hosted-ota-rx: activate failed: ", result);
			return 1;
		}

		context.writeLine("hosted-ota-rx: activate ok");
	} else if (arguments.has("no-activate")) {
		context.writeLine("hosted-ota-rx: activate skipped by --no-activate");
	} else {
		context.writeLine("hosted-ota-rx: activate skipped; current C6 firmware is older than 2.6.0");
	}

	context.writeLine("hosted-ota-rx: update completed");
	if (arguments.has("reboot-host")) {
		context.writeLine("hosted-ota-rx: rebooting host");
		esp_restart();
	} else {
		context.writeLine("hosted-ota-rx: reboot host before using Wi-Fi again");
	}

	return 0;
#else
	(void) arguments;
	context.writeErrorLine("hosted-ota-rx: ESP-Hosted support is unavailable in this build");
	return 1;
#endif
}

} // namespace lexxos::apps::network_programs
