#include <lexxos/apps/fs/ymodem_receive_program.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include <lexxos/app/console_input.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/fs/detail/ymodem_program_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using detail::ymodem::receiveYmodem;
using detail::ymodem::TransferStatus;
using detail::ymodem::writeTransferError;
using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	constexpr ProgramArgumentSpec receiveArguments[] = {
	    {
	     .name = "path",
	     .valueName = "path",
	     .help = "optional VFS destination path; when omitted, the YMODEM file name is used",
	     .kind = ProgramArgumentKind::Positional,
	     .valueType = ProgramArgumentValueType::Path,
	     .required = false,
	     },
	};

} // namespace

ProgramDescriptor YmodemReceiveProgram::descriptor() {
	return {
	    .name = "yrx",
	    .help = "receive a file over YMODEM/CRC",
	    .description = "Receives one YMODEM/CRC file from the raw UART/USB console and writes it to VFS.",
	    .usage = "yrx [path]",
	    .arguments = argumentList(receiveArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = 0,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int YmodemReceiveProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const targetPath = arguments.positionalOr("path", {});
	context.writeLine("yrx: waiting for YMODEM sender");
	context.writeLine("yrx: host example: sb local-file.bin < /dev/ttyACM0 > /dev/ttyACM0");

	auto const result = receiveYmodem(context, targetPath);
	if (result.status != TransferStatus::Ok) {
		writeTransferError(context, "yrx", result);
		return 1;
	}

	context.write("yrx: received ");
	lexxos::apps::writeUnsigned64(context, result.bytes);
	context.writeLine(" bytes");
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
