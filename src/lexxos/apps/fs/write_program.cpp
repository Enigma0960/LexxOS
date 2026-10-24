#include <lexxos/apps/fs/write_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/fs/detail/text_file_writer.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	constexpr lexxos::program::ProgramArgumentSpec writeArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "VFS file path",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = true,
	     },
	    {
         .name = "text",
         .valueName = "text",
         .help = "text to write; extra arguments are joined with spaces",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

ProgramDescriptor WriteProgram::descriptor() {
	return {
	    .name = "write",
	    .help = "write text to a writable VFS file",
	    .description = "Creates or truncates a writable VFS file and writes text joined by spaces. If text is omitted, data is copied from standard input.",
	    .usage = "write <path> [text ...]",
	    .arguments = argumentList(writeArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = detail::textFileWriterWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	    .allowExtraArguments = true,
	};
}

int WriteProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const& arguments) {
	return detail::runTextFileWriter(
	    context,
	    invocation,
	    arguments,
	    lexxos::vfs::OpenMode::CreateTruncateWrite,
	    "write");
}

} // namespace lexxos::apps::filesystem_programs
