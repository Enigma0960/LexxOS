#include <lexxos/apps/fs/move_program.h>

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

	constexpr std::uint32_t moveWorkspaceBytes = static_cast<std::uint32_t>(lexxos::vfs::maxPathLength * 2U);

	constexpr lexxos::program::ProgramArgumentSpec moveArguments[] = {
	    {
         .name = "source",
         .valueName = "source",
         .help = "readable VFS source file",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = true,
	     },
	    {
         .name = "target",
         .valueName = "target",
         .help = "writable VFS target file",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = true,
	     },
	};

	template <typename T>
	bool allocateWorkspaceObject(lexxos::os::AppContext& context, T*& output) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("mv: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(T), alignof(T));
		if (memory == nullptr) {
			context.writeLine("mv: not enough program workspace memory");
			return false;
		}

		output = new (memory) T{};
		return true;
	}

	bool allocateWorkspace(lexxos::os::AppContext& context, PathBuffer*& source, PathBuffer*& target) {
		return allocateWorkspaceObject(context, source) && allocateWorkspaceObject(context, target);
	}

} // namespace

ProgramDescriptor MoveProgram::descriptor() {
	return {
	    .name = "mv",
	    .help = "rename a writable VFS file or directory",
	    .description = "Renames or moves a file or directory inside the same writable filesystem.",
	    .usage = "mv <source> <target>",
	    .arguments = argumentList(moveArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = moveWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int MoveProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* source = nullptr;
	PathBuffer* target = nullptr;
	if (!allocateWorkspace(context, source, target)) {
		return 1;
	}

	// Resolve both endpoints before invoking the VFS rename operation.
	if (!resolvePath(context, arguments.positional("source"), *source)) {
		return 1;
	}
	if (!resolvePath(context, arguments.positional("target"), *target)) {
		return 1;
	}

	// Rename or move the node inside its writable filesystem.
	auto const status = lexxos::vfs::rename(source->data(), target->data());
	if (status != lexxos::vfs::RenameStatus::Ok) {
		context.write("mv: ");
		context.write(source->data());
		context.write(" -> ");
		context.write(target->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(status));
		return 1;
	}
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
