#include <lexxos/apps/fs/stat_program.h>

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

	constexpr std::uint32_t statWorkspaceBytes = static_cast<std::uint32_t>(lexxos::vfs::maxPathLength);

	constexpr lexxos::program::ProgramArgumentSpec statArguments[] = {
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
			context.writeLine("stat: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(PathBuffer), alignof(PathBuffer));
		if (memory == nullptr) {
			context.writeLine("stat: not enough program workspace memory");
			return false;
		}

		path = new (memory) PathBuffer{};
		return true;
	}

} // namespace

ProgramDescriptor StatProgram::descriptor() {
	return {
	    .name = "stat",
	    .help = "show VFS node metadata",
	    .usage = "stat <path>",
	    .arguments = argumentList(statArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = statWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int StatProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* path = nullptr;
	if (!allocatePath(context, path)) {
		return 1;
	}

	// Resolve and inspect the requested node.
	if (!resolvePath(context, arguments.positional("path"), *path)) {
		return 1;
	}

	lexxos::vfs::Stat stat;
	if (!lexxos::vfs::stat(path->data(), stat)) {
		context.write("stat: cannot stat: ");
		context.writeLine(path->data());
		return 1;
	}

	// Print common metadata.
	context.write("path: ");
	context.writeLine(path->data());
	context.write("type: ");
	context.writeLine(lexxos::vfs::toString(stat.type));
	context.write("ready: ");
	context.writeLine(stat.ready ? "yes" : "no");

	// Add block geometry for block devices.
	if (stat.type == lexxos::vfs::NodeType::BlockDevice) {
		context.write("block size: ");
		writeUnsigned(context, stat.blockSizeBytes);
		context.writeLine(" bytes");
		context.write("block count: ");
		writeUnsigned64(context, stat.blockCount);
		context.writeLine("");
	}

	context.write("size: ");
	writeUnsigned64(context, stat.sizeBytes);
	context.writeLine(" bytes");
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
