#include <lexxos/apps/fs/touch_program.h>

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

	constexpr std::uint32_t touchWorkspaceBytes = static_cast<std::uint32_t>(lexxos::vfs::maxPathLength);

	constexpr lexxos::program::ProgramArgumentSpec touchArguments[] = {
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
			context.writeLine("touch: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(PathBuffer), alignof(PathBuffer));
		if (memory == nullptr) {
			context.writeLine("touch: not enough program workspace memory");
			return false;
		}

		path = new (memory) PathBuffer{};
		return true;
	}

} // namespace

ProgramDescriptor TouchProgram::descriptor() {
	return {
	    .name = "touch",
	    .help = "create an empty writable VFS file",
	    .description = "Creates an empty file if it does not exist. Existing files are not truncated.",
	    .usage = "touch <path>",
	    .arguments = argumentList(touchArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = touchWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int TouchProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* path = nullptr;
	if (!allocatePath(context, path)) {
		return 1;
	}

	// Resolve the requested path before opening the file.
	if (!resolvePath(context, arguments.positional("path"), *path)) {
		return 1;
	}

	// Append mode creates a missing file without truncating an existing one.
	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(path->data(), lexxos::vfs::OpenMode::AppendWrite, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		context.write("touch: ");
		context.write(path->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(openStatus));
		return 1;
	}
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
