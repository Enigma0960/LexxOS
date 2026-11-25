#include <lexxos/apps/fs/info_program.h>

#include <array>
#include <new>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/fs/detail/file_info.h>
#include <lexxos/apps/fs/detail/lexx_info.h>
#include <lexxos/program/program_memory_arena.h>

namespace lexxos::apps::filesystem_programs {

using os::AppContext;

namespace {

	using PathBuffer = std::array<char, vfs::maxPathLength>;
	constexpr lexxos::program::ProgramArgumentSpec kArguments[] = {
	    {
	     .name = "path",
	     .valueName = "path",
	     .help = "file or directory path",
	     .kind = lexxos::program::ProgramArgumentKind::Positional,
	     .valueType = lexxos::program::ProgramArgumentValueType::Path,
	     .required = true,
	     }
    };

} // namespace

lexxos::program::ProgramDescriptor InfoProgram::descriptor() {
	return {
	    .name = "info",
	    .help = "show file or directory information",
	    .usage = "info <path>",
	    .arguments = lexxos::apps::argumentList(kArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = lexxos::program::ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = static_cast<std::uint32_t>(sizeof(PathBuffer) + detail::lexxInfoWorkspaceBytes),
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int InfoProgram::run(AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& args) {
	auto* arena = context.memoryArena();
	if (arena == nullptr) {
		context.writeErrorLine("info: program memory arena is unavailable");
		return 1;
	}

	auto* storage = arena->allocate(sizeof(PathBuffer), alignof(PathBuffer));
	if (storage == nullptr) {
		context.writeErrorLine("info: not enough workspace memory");
		return 1;
	}

	auto* path = new (storage) PathBuffer{};
	if (!lexxos::apps::resolvePath(context, args.positional("path"), *path)) {
		return 2;
	}

	lexxos::vfs::Stat stat{};
	if (!context.vfs().stat(path->data(), stat)) {
		context.writeError("info: cannot access: ");
		context.writeErrorLine(path->data());
		return 1;
	}

	auto const result = detail::inspectFileInfo(context, path->data(), stat);
	if (result != 0) {
		return result;
	}

	if (stat.type == vfs::NodeType::RegularFile && detail::isLexxInfoPath(path->data())) {
		return detail::inspectLexxInfo(context, path->data());
	}

	return 0;
}

} // namespace lexxos::apps::filesystem_programs
