#include <lexxos/apps/fs/umount_program.h>

#include <array>
#include <cstdint>
#include <new>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/fs/fat/fat_volume.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;

	constexpr std::uint32_t umountWorkspaceBytes = static_cast<std::uint32_t>(lexxos::vfs::maxPathLength);

	constexpr lexxos::program::ProgramArgumentSpec umountArguments[] = {
	    {
         .name = "target",
         .valueName = "target",
         .help = "VFS mount target path",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = true,
	     },
	};

	bool allocatePath(lexxos::os::AppContext& context, PathBuffer*& path) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("umount: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(PathBuffer), alignof(PathBuffer));
		if (memory == nullptr) {
			context.writeLine("umount: not enough program workspace memory");
			return false;
		}

		path = new (memory) PathBuffer{};
		return true;
	}

} // namespace

ProgramDescriptor UmountProgram::descriptor() {
	return {
	    .name = "umount",
	    .help = "unmount a VFS mount point",
	    .usage = "umount <target>",
	    .arguments = argumentList(umountArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = umountWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int UmountProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* target = nullptr;
	if (!allocatePath(context, target)) {
		return 1;
	}

	// Resolve the mount point before calling the FAT unmount hook.
	if (!resolvePath(context, arguments.positional("target"), *target)) {
		return 1;
	}

	// Unmount and preserve the VFS status in diagnostics.
	auto const status = lexxos::fs::fat::unmount(target->data());
	if (status != lexxos::vfs::UnmountStatus::Ok) {
		context.write("umount: ");
		context.write(target->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(status));
		return 1;
	}
	context.write("unmounted ");
	context.writeLine(target->data());
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
