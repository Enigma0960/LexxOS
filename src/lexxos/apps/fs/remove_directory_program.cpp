#include <lexxos/apps/fs/remove_directory_program.h>

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

	constexpr std::uint32_t removeDirectoryWorkspaceBytes = static_cast<std::uint32_t>(lexxos::vfs::maxPathLength);

	constexpr lexxos::program::ProgramArgumentSpec removeDirectoryArguments[] = {
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
			context.writeLine("rmdir: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(PathBuffer), alignof(PathBuffer));
		if (memory == nullptr) {
			context.writeLine("rmdir: not enough program workspace memory");
			return false;
		}

		path = new (memory) PathBuffer{};
		return true;
	}

} // namespace

ProgramDescriptor RemoveDirectoryProgram::descriptor() {
	return {
	    .name = "rmdir",
	    .help = "remove an empty writable VFS directory",
	    .description = "Removes an empty directory on writable filesystems. FAT supports short 8.3 names and long names when the backend is configured with LFN support.",
	    .usage = "rmdir <path>",
	    .arguments = argumentList(removeDirectoryArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = removeDirectoryWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int RemoveDirectoryProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* path = nullptr;
	if (!allocatePath(context, path)) {
		return 1;
	}

	// Resolve the directory path before changing the filesystem.
	if (!resolvePath(context, arguments.positional("path"), *path)) {
		return 1;
	}

	// Remove the empty directory and report the backend status.
	auto const status = lexxos::vfs::removeDirectory(path->data());
	if (status != lexxos::vfs::RemoveStatus::Ok) {
		context.write("rmdir: ");
		context.write(path->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(status));
		return 1;
	}
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
