#include <lexxos/apps/fs/make_directory_program.h>

#include <array>
#include <cstdint>
#include <new>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;

	constexpr std::uint32_t makeDirectoryWorkspaceBytes = static_cast<std::uint32_t>(lexxos::vfs::maxPathLength);

	constexpr lexxos::program::ProgramArgumentSpec makeDirectoryArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "required VFS path",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = true,
	     },
	};

	bool allocatePath(lexxos::os::AppContext& context, PathBuffer*& path) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("mkdir: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(PathBuffer), alignof(PathBuffer));
		if (memory == nullptr) {
			context.writeLine("mkdir: not enough program workspace memory");
			return false;
		}

		path = new (memory) PathBuffer{};
		return true;
	}

} // namespace

ProgramDescriptor MakeDirectoryProgram::descriptor() {
	return {
	    .name = "mkdir",
	    .help = "create a writable VFS directory",
	    .description = "Creates a directory on writable filesystems. FAT supports short 8.3 names and ASCII long names.",
	    .usage = "mkdir <path>",
	    .arguments = argumentList(makeDirectoryArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = makeDirectoryWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int MakeDirectoryProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* path = nullptr;
	if (!allocatePath(context, path)) {
		return 1;
	}

	// Resolve the target directory path.
	if (!resolvePath(context, arguments.positional("path"), *path)) {
		return 1;
	}

	// Create the directory and report the backend status.
	auto const status = lexxos::vfs::createDirectory(path->data());
	if (status != lexxos::vfs::WriteStatus::Ok) {
		context.write("mkdir: ");
		context.write(path->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(status));
		return 1;
	}
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
