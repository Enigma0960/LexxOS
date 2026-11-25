#include <lexxos/apps/fs/ymodem_send_program.h>

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

using detail::ymodem::basenameOf;
using detail::ymodem::isSafeReceivedName;
using detail::ymodem::PathBuffer;
using detail::ymodem::sendYmodem;
using detail::ymodem::toString;
using detail::ymodem::TransferStatus;
using detail::ymodem::writeTransferError;
using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	constexpr ProgramArgumentSpec sendArguments[] = {
	    {
	     .name = "path",
	     .valueName = "path",
	     .help = "VFS source file path",
	     .kind = ProgramArgumentKind::Positional,
	     .valueType = ProgramArgumentValueType::Path,
	     .required = true,
	     },
	    {
	     .name = "name",
	     .valueName = "name",
	     .help = "optional transmitted file name",
	     .kind = ProgramArgumentKind::Positional,
	     .valueType = ProgramArgumentValueType::String,
	     .required = false,
	     },
	};

} // namespace

ProgramDescriptor YmodemSendProgram::descriptor() {
	return {
	    .name = "ysend",
	    .help = "send a VFS file over YMODEM/CRC",
	    .description = "Sends one VFS file to the raw UART/USB console using YMODEM/CRC.",
	    .usage = "ysend <path> [name]",
	    .arguments = argumentList(sendArguments),
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

int YmodemSendProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer path{};
	if (!resolvePath(context, arguments.positional("path"), path)) {
		return 1;
	}

	auto transmittedName = arguments.positionalOr("name", {});
	if (transmittedName.empty()) {
		transmittedName = basenameOf(path.data());
	}

	if (!isSafeReceivedName(transmittedName)) {
		context.writeLine("ysend: invalid transmitted file name");
		return 1;
	}

	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path.data(), lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("ysend: ");
		context.write(path.data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return 1;
	}

	context.writeLine("ysend: waiting for YMODEM receiver");
	context.writeLine("ysend: host example: rb < /dev/ttyACM0 > /dev/ttyACM0");

	auto const result = sendYmodem(context, file, transmittedName);
	if (result.status != TransferStatus::Ok) {
		writeTransferError(context, "ysend", result);
		return 1;
	}

	context.write("ysend: sent ");
	lexxos::apps::writeUnsigned64(context, result.bytes);
	context.writeLine(" bytes");
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
