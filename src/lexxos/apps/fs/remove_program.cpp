#include <lexxos/apps/fs/remove_program.h>

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

	constexpr std::uint32_t removeWorkspaceBytes = static_cast<std::uint32_t>(lexxos::vfs::maxPathLength);

	constexpr lexxos::program::ProgramArgumentSpec removeArguments[] = {
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
			context.writeLine("rm: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(PathBuffer), alignof(PathBuffer));
		if (memory == nullptr) {
			context.writeLine("rm: not enough program workspace memory");
			return false;
		}

		path = new (memory) PathBuffer{};
		return true;
	}

} // namespace

ProgramDescriptor RemoveProgram::descriptor() {
	return {
	    .name = "rm",
	    .help = "remove a writable VFS file",
	    .description = "Removes writable VFS files. This is supported for /tmp files and regular files on writable FAT mounts.",
	    .usage = "rm <path>",
	    .arguments = argumentList(removeArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = removeWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int RemoveProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* path = nullptr;
	if (!allocatePath(context, path)) {
		return 1;
	}

	// Resolve the target before invoking the VFS mutation.
	if (!resolvePath(context, arguments.positional("path"), *path)) {
		return 1;
	}

	// Remove the file and preserve the backend status in diagnostics.
	auto const status = lexxos::vfs::remove(path->data());
	if (status != lexxos::vfs::RemoveStatus::Ok) {
		context.write("rm: ");
		context.write(path->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(status));
		return 1;
	}
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
